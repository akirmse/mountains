# Incrementally build up a polygon as a union of other polygons.
# Overviews (minified versions) of polygons are used if present.

from osgeo import gdal, ogr, osr

class Boundary:
    # Overall boundary
    boundary = ogr.Geometry(ogr.wkbMultiPolygon)
    
    # Current batch of polygons, to be added to "boundary" when
    # its size reaches batch_size
    batch_size = 100
    batch_boundary = ogr.Geometry(ogr.wkbMultiPolygon)
    batch_boundary_size = 0

    def add_dataset(self, filename):
        # Look for overviews
        raw_dataset = gdal.Open(filename)
        if raw_dataset is None:
            print("Couldn't open dataset", filename)
            return
        
        num_overviews = raw_dataset.GetRasterBand(1).GetOverviewCount()
        if num_overviews == 0:
            print("Data has no overviews; using full resolution (slow)")
            overview_level = None
        else:
            overview_level = min(2, num_overviews-1)
        raw_dataset = None

        # Rarely, an image will report having overviews, but gdal.Footprint can't
        # find them (likely a gdal bug).  If so, retry with no overview.
        footprint = None
        while True:
            try:
                footprint_options = gdal.FootprintOptions(
                    ovr=overview_level, dstSRS='EPSG:4326', maxPoints=10000,
                    callback=gdal.TermProgress_nocb)
                dataset = gdal.Open(filename)
                footprint = gdal.Footprint("/vsimem/outline.shp", dataset, options=footprint_options)
                break
            except RuntimeError as e:
                if overview_level is None:
                    print(f"Failed to read overviews, even though they're present")
                    raise
                print("Couldn't read overviews; using full resolution (slow)")
                overview_level = None
            
        if footprint:
            feature = footprint.GetLayer(0).GetNextFeature()
            if feature:  # Tiles can be completely empty
                geometry = feature.geometry()
                # Very rarely, there can be self-intersection.  Try to fix it.
                if not geometry.IsValid():
                    geometry = geometry.MakeValid()
                self.batch_boundary = self.batch_boundary.Union(geometry)
                self.batch_boundary_size += 1
                if self.batch_boundary_size >= self.batch_size:
                    self.boundary = self.boundary.Union(self.batch_boundary)
                    self.batch_boundary = ogr.Geometry(ogr.wkbMultiPolygon)
                    self.batch_boundary_size = 0
                
        footprint = None  # Close file

    def write_to_file(self, filename):
        driver = ogr.GetDriverByName("ESRI Shapefile")
        # create the data source
        ds = driver.CreateDataSource(filename)
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)

        # create one layer with an ID field
        layer = ds.CreateLayer("boundary", srs, ogr.wkbMultiPolygon)
        idField = ogr.FieldDefn("id", ogr.OFTInteger)
        layer.CreateField(idField)

        # Create the feature and set values
        featureDefn = layer.GetLayerDefn()
        feature = ogr.Feature(featureDefn)
        feature.SetGeometry(self.get_boundary())
        feature.SetField("id", 1)
        layer.CreateFeature(feature)
        
        feature = None
        ds = None
        
    def get_boundary(self):
        # Merge in any last batch
        if self.batch_boundary_size > 0:
            self.boundary = self.boundary.Union(self.batch_boundary)
            self.batch_boundary_size = 0

        return self.boundary
        
