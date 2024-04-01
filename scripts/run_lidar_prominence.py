# Processes a set of Lidar tiles:
# - Converts them to lat-long projection
# - Resamples to tiles that are of fixed size and resolution, with elevations in meters
#
# Writes a shapefiles "boundary.shp" to the tile dir, showing the nodata
# boundary of the input.
#
# Requires osgeo in the Python path

# TODO: Add an -n option to skip running anything

import argparse
import glob
import math
import os
import signal
import subprocess

from interrupt import handle_ctrl_c, init_pool
from multiprocessing import Pool
from osgeo import gdal, ogr, osr
from pathlib import Path

# Each output tile is this many degrees and samples on a side
TILE_SIZE_DEGREES = 0.1  # Must divide 1 evenly
TILE_SIZE_SAMPLES = 10000

epsilon = 0.0001

def run_command(command_string):
    print("> " + command_string)
    retval = subprocess.call(command_string, shell=True)

def maybe_create_directory(dir_name):
    if os.path.isdir(dir_name):
        return
    os.mkdir(dir_name)
    if not os.path.isdir(dir_name):
        print(f"Couldn't create directory {dir_name}")
    
def round_down(coord):
    """Return coord rounded down to the nearest TILE_SIZE_DEGREES"""
    return math.floor(coord / TILE_SIZE_DEGREES) * TILE_SIZE_DEGREES

def round_up(coord):
    """Return coord rounded up to the nearest TILE_SIZE_DEGREES"""
    return math.ceil(coord / TILE_SIZE_DEGREES) * TILE_SIZE_DEGREES

def sign(a):
    return bool(a > 0) - bool(a < 0)

def filename_for_coordinates(x, y):
    """Return output filename for the given coordinates"""
    y += TILE_SIZE_DEGREES  # Name uses upper left corner
    xpart = int(round(x * 100))
    ypart = int(round(y * 100))

    x_int = int(float(xpart) / 100)
    y_int = int(float(ypart) / 100)
    x_fraction = abs(xpart) % 100
    y_fraction = abs(ypart) % 100

    # Handle "-0.1" -> "-00x10"
    if xpart < 0:
        x_string = f"-{abs(x_int):02d}"
    else:
        x_string = f"{x_int:03d}"
    x_string +=  f"x{x_fraction:02d}"

    if ypart < 0:
        y_string = f"-{abs(y_int):02d}"
    else:
        y_string = f"{y_int:02d}"
    y_string +=  f"x{y_fraction:02d}"

    return f"tile_{y_string}_{x_string}.flt"

def write_multipolygon_to_file(multipolygon, filename):
    driver = ogr.GetDriverByName("ESRI Shapefile")
    ds = driver.CreateDataSource(filename)

    srs =  osr.SpatialReference()
    srs.ImportFromEPSG(4326)

    layer = ds.CreateLayer("boundary", srs, ogr.wkbMultiPolygon)
    
    idField = ogr.FieldDefn("id", ogr.OFTInteger)
    layer.CreateField(idField)
    
    # Create the feature and set values
    featureDefn = layer.GetLayerDefn()
    feature = ogr.Feature(featureDefn)
    feature.SetGeometry(multipolygon)
    feature.SetField("id", 1)
    layer.CreateFeature(feature)
    
    feature = None
    ds = None

def polygon_for_tile(x, y, xsize, ysize):
    """Return a rectangle with (x,y) as its lower left corner, size degrees in both directions"""
    box = ogr.Geometry(ogr.wkbLinearRing)
    box.AddPoint(x, y)
    box.AddPoint(x, y + ysize)
    box.AddPoint(x + xsize, y + ysize)
    box.AddPoint(x + xsize, y)
    box.AddPoint(x, y)
    polybox = ogr.Geometry(ogr.wkbPolygon)
    polybox.AddGeometry(box)
    return polybox

def get_extent(ds):
    """ Return list of corner coordinates from a gdal Dataset """
    xmin, xpixel, _, ymax, _, ypixel = ds.GetGeoTransform()
    width, height = ds.RasterXSize, ds.RasterYSize
    xmax = xmin + width * xpixel
    ymin = ymax + height * ypixel

    return xmin, xmax, ymin, ymax

def create_vrts(tile_dir, input_files, skip_boundary):
    """
    Creates virtual rasters (VRTs) for the input files on disk.
    Returns:
    1) The filename for a VRT of all of the input files in EPSG:4326
    2) The bounding polygon for all of the input files in EPSG:4326
    """
    print("Creating VRTs and computing boundary")

    # Some input datasets have more than one projection.
    #
    # All files in a VRT must have the same projection.  Thus, we must first
    # create an intermediate VRT that warps the inputs.
    #
    # However, computing the boundary can be much faster on the raw
    # data, because we can use overview (pyramid) images at lower
    # resolution, and these would not be present in the warped
    # version.  So, for each projection, we create a VRT of all the
    # inputs with that projection, compute the boundary of each one,
    # and then we take the union of all of these boundaries to get the
    # boundary of all of the inputs.
    
    # Map of projection name to list of files with that projection
    projection_map = {}  
    for input_file in input_files:
        ds = gdal.Open(input_file)
        prj = ds.GetProjection()
        srs = osr.SpatialReference(wkt=prj)
        projection_name = srs.GetAttrValue('projcs')
        projection_map.setdefault(projection_name, []).append(input_file)

    # For each projection, build raw and warped VRTs
    index = 1
    boundary = ogr.Geometry(ogr.wkbMultiPolygon)
    warped_vrt_filenames = []
    for proj, filenames in projection_map.items():
        print(f"Projection {index}: {proj}")
        print("  Creating VRTs")
        raw_vrt_filename = os.path.join(tile_dir, f"raw{index}.vrt")
        warped_vrt_filename = os.path.join(tile_dir, f"warped{index}.vrt")

        vrt_options = gdal.BuildVRTOptions(callback=gdal.TermProgress_nocb)
        gdal.BuildVRT(raw_vrt_filename, filenames, options=vrt_options)
        
        warp_options = gdal.WarpOptions(format = "VRT", dstSRS = 'EPSG:4326')
        gdal.Warp(warped_vrt_filename, raw_vrt_filename, options = warp_options,
                  callback=gdal.TermProgress_nocb)
        warped_vrt_filenames.append(warped_vrt_filename)
                  
        # If there are overviews, it's much faster to use them.  Only the
        # raw (unwarped) VRT will preserve the overviews from the source rasters.
        raw_dataset = gdal.Open(raw_vrt_filename)
        num_overviews = raw_dataset.GetRasterBand(1).GetOverviewCount()
        if num_overviews == 0:
            print("Data has no overviews; using full resolution (slow)")
            overview_level = None
        else:
            overview_level = min(2, num_overviews-1)
        raw_dataset = None

        if not skip_boundary:
            print("  Computing boundary")
            footprint_filename = os.path.join(tile_dir, f'boundary{index}.shp')
            footprint_options = gdal.FootprintOptions(
                ovr=overview_level, dstSRS='EPSG:4326', maxPoints=10000,
                callback=gdal.TermProgress_nocb)
            footprint = gdal.Footprint(footprint_filename, raw_vrt_filename, options=footprint_options)
            
            geometry = footprint.GetLayer(0).GetNextFeature()
            boundary = boundary.Union(geometry.geometry())
            footprint = None  # Close file

        index += 1

    # Build a single VRT of all the warped VRTs
    overall_vrt_filename = os.path.join(tile_dir, 'all.vrt')
    vrt_options = gdal.BuildVRTOptions(callback=gdal.TermProgress_nocb)
    gdal.BuildVRT(overall_vrt_filename, warped_vrt_filenames, options=vrt_options)

    if skip_boundary:
        # Get crude rectangular boundary
        ds = gdal.Open(overall_vrt_filename)
        xmin, xmax, ymin, ymax = get_extent(ds)
        boundary = polygon_for_tile(xmin, ymin, xmax - xmin, ymax - ymin)
    else:
        # Write boundary to file for debugging / reuse.
        write_multipolygon_to_file(boundary, os.path.join(tile_dir, "boundary.shp"))

    return overall_vrt_filename, boundary

@handle_ctrl_c
def process_tile(args):
    """scale is multiplied by each input value"""
    (x, y, vrt_filename, output_filename, scale) = args
    print(f"Processing {x:.2f}, {y:.2f}")
    gdal.UseExceptions()

    translate_options = gdal.TranslateOptions(
        format = "EHdr",
        width = TILE_SIZE_SAMPLES, height = TILE_SIZE_SAMPLES,
        projWin = [x, y + TILE_SIZE_DEGREES, x + TILE_SIZE_DEGREES, y],
        noData = -999999,
        scaleParams = [[-30000, 30000, -30000*scale, 30000*scale]],
        callback=gdal.TermProgress_nocb)
    gdal.Translate(output_filename, vrt_filename, options = translate_options)

def main():
    parser = argparse.ArgumentParser(description='Convert LIDAR to standard tiles')
    requiredNamed = parser.add_argument_group('required named arguments')
    parser.add_argument('--input_units', choices=['feet', 'meters'],
                        default='meters',
                        help="Elevation units in input files")
    parser.add_argument('--output_units', choices=['feet', 'meters'],
                        default='meters',
                        help="Elevation units in final output files")
    parser.add_argument('--output_dir', default="prominence",
                        help="Directory to place prominence output")
    parser.add_argument('--tile_dir', default="tiles",
                        help="Directory to place warped tiles")
    parser.add_argument('--binary_dir', default='release',
                        help="Path to prominence binary")
    parser.add_argument('--threads', default=1, type=int,
                        help="Number of threads to use in computing prominence")
    parser.add_argument('--min_prominence', default=100, type=float,
                        help="Filter to this minimum prominence in meters")
    parser.add_argument('--skip_boundary', action=argparse.BooleanOptionalAction,
                        help="Skip computation of raster boundary; uses more disk")
    parser.add_argument('input_files', type=str, nargs='+',
                        help='Input Lidar tiles, or GDAL VRT of tiles')

# TODO: Allow boundary to be specified from a file along with overall VRT file
#    parser.add_argument('--boundary',
#                        help="Precomputed shp file with boundary of input data")
    args = parser.parse_args()

    gdal.UseExceptions()

    # Create missing directories
    maybe_create_directory(args.output_dir)
    maybe_create_directory(args.tile_dir)
    
    # Treat each input as potentially a glob, and then flatten the list
    input_files = []
    for filespec in args.input_files:
        files = glob.glob(filespec)
        if len(files) == 0:
            print(f"Input filespec {filespec} matched no files; exiting.")
            exit(1)
        input_files.extend(files)

    # Build all the VRTs and compute boundary for entire input
    warped_vrt_filename, bounds = create_vrts(args.tile_dir, input_files,
                                              args.skip_boundary)
    xmin, xmax, ymin, ymax = bounds.GetEnvelope()

    # Rounded to tile degree boundaries so that we cover the whole data set
    xmin = round_down(xmin)
    ymin = round_down(ymin)
    xmax = round_up(xmax)
    ymax = round_up(ymax)

    # Run in parallel
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    pool = Pool(args.threads, initializer=init_pool)

    print("Generating output tiles")
    # Extract tiles from input VRT
    process_args = []
    y = ymin
    while y <= ymax - epsilon:
        x = xmin
        while x <= xmax - epsilon:
            # Skip this tile if it doesn't overlap any data in the source
            polybox = polygon_for_tile(x, y, TILE_SIZE_DEGREES, TILE_SIZE_DEGREES)
            if args.skip_boundary or polybox.Intersects(bounds):
                output_filename = os.path.join(args.tile_dir, filename_for_coordinates(x, y))
                # If input is in feet, will need to scale tile to meters
                if args.input_units == "feet":
                    scale = 0.3048
                else:
                    scale = 1

                process_args.append((x, y, warped_vrt_filename, output_filename, scale))
            
            x += TILE_SIZE_DEGREES
        y += TILE_SIZE_DEGREES

    results = pool.map(process_tile, process_args)
    if any(map(lambda x: isinstance(x, KeyboardInterrupt), results)):
       print('Ctrl-C was entered.')
       exit(1)

    pool.close()
    pool.join()

    # Run prominence and merge_divide_trees
    print("Running prominence")
    prominence_binary = os.path.join(args.binary_dir, "prominence")
    prom_command = f"{prominence_binary} --v=1 -f LIDAR -i {args.tile_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {args.min_prominence}" + \
        f" -- {ymin} {ymax} {xmin} {xmax}"
    run_command(prom_command)

    print("Running merge")
    merge_binary = os.path.join(args.binary_dir, "merge_divide_trees")
    merge_inputs = os.path.join(args.output_dir, "*_pruned_*.dvt")
    merge_output = os.path.join(args.output_dir, "results")
    units_multiplier = 1
    if args.output_units == "feet":
        units_multiplier = 3.28084
    merge_command = f"{merge_binary} --v=1 -f" + \
        f" -t {args.threads} -m {args.min_prominence} -s {units_multiplier}" + \
        f" {merge_output} {merge_inputs}"
    run_command(merge_command)

if __name__ == '__main__':
    main()
