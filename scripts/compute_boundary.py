# Compute boundary shapefile of a set of given input rasters

import argparse
import glob
import math
import os
import signal
import subprocess

from osgeo import gdal, ogr, osr

def process_files(files):
    print("  Computing boundary")
    boundary = ogr.Geometry(ogr.wkbMultiPolygon)
    geometry = None
    for f in files:
        print(f)
        # Look for overviews
        raw_dataset = gdal.Open(f)
        if raw_dataset is None:
            print("Couldn't open dataset")
            continue
        
        num_overviews = raw_dataset.GetRasterBand(1).GetOverviewCount()
        if num_overviews == 0:
            print("Data has no overviews; using full resolution (slow)")
            overview_level = None
        else:
            overview_level = min(2, num_overviews-1)
        raw_dataset = None

        footprint_options = gdal.FootprintOptions(
            ovr=overview_level, dstSRS='EPSG:4326', maxPoints=10000,
            callback=gdal.TermProgress_nocb)
        dataset = gdal.Open(f)
        footprint = gdal.Footprint("/vsimem/outline.shp", dataset, options=footprint_options)
        if footprint:
            geometry = footprint.GetLayer(0).GetNextFeature()
            if geometry:  # Tiles can be completely empty
                boundary = boundary.Union(geometry.geometry())
        footprint = None  # Close file

    # Write merged geometry
    if geometry:
        footprint_filename = 'boundary.shp'
        driver = ogr.GetDriverByName("ESRI Shapefile")
        # create the data source
        ds = driver.CreateDataSource(footprint_filename)
        srs = osr.SpatialReference()
        srs.ImportFromEPSG(4326)

        # create one layer with an ID field
        layer = ds.CreateLayer("boundary", srs, ogr.wkbMultiPolygon)
        idField = ogr.FieldDefn("id", ogr.OFTInteger)
        layer.CreateField(idField)

        # Create the feature and set values
        featureDefn = layer.GetLayerDefn()
        feature = ogr.Feature(featureDefn)
        feature.SetGeometry(boundary)
        feature.SetField("id", 1)
        layer.CreateFeature(feature)
        
        feature = None
        ds = None


def main():
    parser = argparse.ArgumentParser(description='Convert LIDAR to standard tiles')
    parser.add_argument('input_files', type=str, nargs='+',
                        help='Input Lidar tiles, or GDAL VRT of tiles')
    args = parser.parse_args()

    # Treat each input as potentially a glob, and then flatten the list
    input_files = []
    for filespec in args.input_files:
        files = glob.glob(filespec)
        if len(files) == 0:
            print(f"Input filespec {filespec} matched no files; exiting.")
            exit(1)
        input_files.extend(files)

    process_files(input_files)
        
if __name__ == '__main__':
    main()
