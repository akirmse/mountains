# Processes a set of Lidar tiles:
# - Converts them to lat-long projection
# - Resamples to tiles that are of fixed size and resolution
#
# Writes a shapefiles "boundary.shp" to the tile dir, showing the nodata
# boundary of the input.
#
# Requires osgeo in the Python path

# TODO: Add an -n option to skip running anything

import argparse
import glob
import itertools
import math
import os
import signal
import subprocess

from interrupt import handle_ctrl_c, init_pool
from multiprocessing import Pool
from osgeo import gdal, ogr

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

def filename_for_coordinates(x, y):
    """Return output filename for the given coordinates"""
    y += TILE_SIZE_DEGREES  # Name uses upper left corner
    x_int = int(x)
    y_int = int(y)
    x_fraction = int(abs(100 * (x - x_int)) + epsilon)
    y_fraction = int(abs(100 * (y - y_int)) + epsilon)
    return f"tile_{y_int:02d}x{y_fraction:02d}_{x_int:03d}x{x_fraction:02d}.flt"

@handle_ctrl_c
def process_tile(args):
    (x, y, vrt_filename, output_filename) = args
    print(f"Processing {x:.2f}, {y:.2f}")
    gdal.UseExceptions()
    translate_options = gdal.TranslateOptions(
        format = "EHdr",
        width = TILE_SIZE_SAMPLES, height = TILE_SIZE_SAMPLES,
        projWin = [x, y + TILE_SIZE_DEGREES, x + TILE_SIZE_DEGREES, y],
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
                        help="Filter to this minimum prominence (units are --input_units")
    parser.add_argument('input_files', type=str, nargs='+',
                        help='Input Lidar tiles, or GDAL VRT of tiles')

    parser.add_argument('--boundary',
                        help="Precomputed shp file with boundary of input data")
    args = parser.parse_args()

    gdal.UseExceptions()

    # Create missing directories
    maybe_create_directory(args.output_dir)
    maybe_create_directory(args.tile_dir)
    
    # Treat each input as potentially a glob, and then flatten the list
    input_files = [ glob.glob(x) for x in args.input_files ]
    input_files = list(itertools.chain.from_iterable(input_files))

    print("Creating virtual raster")

    # Input is a VRT?
    if len(args.input_files) == 1 and args.input_files[0].lower().endswith(".vrt"):
        raw_vrt_filename = args.input_files[0]
    else:
        # Create raw VRT for all inputs
        raw_vrt_filename = os.path.join(args.tile_dir, 'raw.vrt')
        vrt_options = gdal.BuildVRTOptions(callback=gdal.TermProgress_nocb)
        gdal.BuildVRT(raw_vrt_filename, input_files, options=vrt_options)
    
    # Reproject VRT
    warped_vrt_filename = os.path.join(args.tile_dir, 'warped.vrt')
    warp_options = gdal.WarpOptions(format = "VRT", dstSRS = 'EPSG:4326')
    gdal.Warp(warped_vrt_filename, raw_vrt_filename, options = warp_options,
              callback=gdal.TermProgress_nocb)

    # Get bounding polygon of source data
    print("Computing bounding area")

    if args.boundary:  # Boundary file manually specified?
        footprint = ogr.Open(args.boundary)
    else:
        # If there are overviews, it's much faster to use them
        raw_dataset = gdal.Open(raw_vrt_filename)
        num_overviews = raw_dataset.GetRasterBand(1).GetOverviewCount()
        if num_overviews == 0:
            print("Data has no overviews; using full resolution (slow)")
            overview_level = None
        else:
            overview_level = min(2, num_overviews)
        raw_dataset = None

        footprint_filename = os.path.join(args.tile_dir, 'boundary.shp')
        footprint_options = gdal.FootprintOptions(
            ovr=overview_level, dstSRS='EPSG:4326', maxPoints=10000,
            callback=gdal.TermProgress_nocb)
        footprint = gdal.Footprint(footprint_filename, raw_vrt_filename, options=footprint_options)

    bounds = footprint.GetLayer(0).GetNextFeature()
    coverage_geometry = bounds.GetGeometryRef()
    xmin, xmax, ymin, ymax = coverage_geometry.GetEnvelope()
    footprint = None  # Close file

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
            box = ogr.Geometry(ogr.wkbLinearRing)
            box.AddPoint(x, y)
            box.AddPoint(x, y + TILE_SIZE_DEGREES)
            box.AddPoint(x + TILE_SIZE_DEGREES, y + TILE_SIZE_DEGREES)
            box.AddPoint(x + TILE_SIZE_DEGREES, y)
            box.AddPoint(x, y)
            polybox = ogr.Geometry(ogr.wkbPolygon)
            polybox.AddGeometry(box)
            if polybox.Intersects(coverage_geometry):
                output_filename = os.path.join(args.tile_dir, filename_for_coordinates(x, y))
                process_args.append((x, y, warped_vrt_filename, output_filename))
            
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
    # Find more peaks than ultimately desired, so that we regenerate a final list
    # by simply running merge_divide_trees again (this is a personal preference).
    low_min_prominence = args.min_prominence / 3
    prom_command = f"{prominence_binary} --v=1 -f LIDAR -i {args.tile_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {low_min_prominence}" + \
        f" -- {ymin} {ymax} {xmin} {xmax}"
    run_command(prom_command)

    print("Running merge")
    merge_binary = os.path.join(args.binary_dir, "merge_divide_trees")
    merge_inputs = os.path.join(args.output_dir, "*_*.dvt")
    merge_output = os.path.join(args.output_dir, "results")
    units_multiplier = 1
    if args.input_units == "feet" and args.output_units == "meters":
        units_multiplier = 1.0 / 3.28084
    if args.input_units == "meters" and args.output_units == "feet":
        units_multiplier = 3.28084
    merge_command = f"{merge_binary} --v=1 -f" + \
        f" -t {args.threads} -m {args.min_prominence} -s {units_multiplier}" + \
        f" {merge_output} {merge_inputs}"
    run_command(merge_command)

if __name__ == '__main__':
    main()
