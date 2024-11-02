# Processes a set of raster tiles:
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

from boundary import Boundary

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
    
def round_down(coord, interval):
    """Return coord rounded down to the nearest interval"""
    return math.floor(coord / interval) * interval

def round_up(coord, interval):
    """Return coord rounded up to the nearest interval"""
    return math.ceil(coord / interval) * interval

def sign(a):
    return bool(a > 0) - bool(a < 0)

def filename_for_coordinates(x, y, degrees_per_tile):
    """Return output filename for the given coordinates"""
    y += degrees_per_tile  # Name uses upper left corner
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

    # With many thousands of files, continually merging into one polygon
    # gets slow.  So instead we do it in batches.
    boundary = Boundary()
    warped_vrt_filenames = []
    for proj, filenames in projection_map.items():
        print(f"Projection {index}: {proj}")
        print("  Creating VRTs")
        raw_vrt_filename = os.path.join(tile_dir, f"raw{index}.vrt")
        warped_vrt_filename = os.path.join(tile_dir, f"warped{index}.vrt")

        raw_options = gdal.BuildVRTOptions(callback=gdal.TermProgress_nocb)
        gdal.BuildVRT(raw_vrt_filename, filenames, options=raw_options)
        
        warp_options = gdal.WarpOptions(format = "VRT", dstSRS = 'EPSG:4326')
        warped_dataset = gdal.Warp(warped_vrt_filename, raw_vrt_filename, options = warp_options,
                                   callback=gdal.TermProgress_nocb)
        # BuildVRT requires that all sources have the same color interpretation, so set
        # it to grayscale.  It's sometimes missing in some datasets, which isn't allowed.
        band = warped_dataset.GetRasterBand(1)
        band.SetColorInterpretation(gdal.GCI_GrayIndex)
        warped_dataset = None  # Close file
        warped_vrt_filenames.append(warped_vrt_filename)

        if not skip_boundary:
            print("  Computing boundary")
            for tile in filenames:
                print(tile)
                boundary.add_dataset(tile)
        index += 1

    # Build a single VRT of all the warped VRTs
    overall_vrt_filename = os.path.join(tile_dir, 'all.vrt')
    vrt_options = gdal.BuildVRTOptions(callback=gdal.TermProgress_nocb)
    gdal.BuildVRT(overall_vrt_filename, warped_vrt_filenames, options=vrt_options)

    if skip_boundary:
        # Get crude rectangular boundary
        ds = gdal.Open(overall_vrt_filename)
        xmin, xmax, ymin, ymax = get_extent(ds)
        polygon = polygon_for_tile(xmin, ymin, xmax - xmin, ymax - ymin)
    else:
        # Write boundary to file for debugging / reuse.
        boundary.write_to_file(os.path.join(tile_dir, "boundary.shp"))
        polygon = boundary.get_boundary()

    return overall_vrt_filename, polygon

@handle_ctrl_c
def process_tile(args):
    """scale is multiplied by each input value"""
    (x, y, vrt_filename, output_filename, scale, degrees_per_tile, samples_per_tile) = args
    print(f"Processing {x:.2f}, {y:.2f}")
    gdal.UseExceptions()

    translate_options = gdal.TranslateOptions(
        format = "EHdr",
        width = samples_per_tile, height = samples_per_tile,
        projWin = [x, y + degrees_per_tile, x + degrees_per_tile, y],
        noData = -999999,
        scaleParams = [[-30000, 30000, -30000*scale, 30000*scale]],
        callback=gdal.TermProgress_nocb)
    gdal.Translate(output_filename, vrt_filename, options = translate_options)

def main():
    parser = argparse.ArgumentParser(description='Reproject arbitrary rasters to a fixed grid and compute prominence')
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
    parser.add_argument('--boundary',
                        help="Precomputed shp file with boundary of input data")
    parser.add_argument('--degrees_per_tile', type=float, default=0.1,
                        help='Size of reprojected tiles to generate')
    parser.add_argument('--samples_per_tile', type=int, default=10000,
                        help='Number of samples per edge of a reprojected tile')
    parser.add_argument('--bathymetry', action=argparse.BooleanOptionalAction,
                        help='Is the input DEM bathymetry?')
    args = parser.parse_args()

    gdal.UseExceptions()

    # Valudate degrees per tile; it must divide 1 degree evenly
    tiles_per_degree = 1 / args.degrees_per_tile
    if abs(int(tiles_per_degree) - tiles_per_degree) > 0.001:
        print("tiles_per_degree must divide 1 degree evenly")
        exit(1)
    
    # Create missing directories
    maybe_create_directory(args.output_dir)
    maybe_create_directory(args.tile_dir)
    
    # If a boundary is specified, don't compute a new one
    skip_computing_boundary = args.skip_boundary
    if args.boundary:
        skip_computing_boundary = True
        ds = ogr.Open(args.boundary)
        layer = ds.GetLayer(0)
        feat = layer.GetFeature(0)
        manually_specified_boundary = feat.GetGeometryRef()
            
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
                                              skip_computing_boundary)
    # Boundary manually specified?
    if args.boundary:
        bounds = manually_specified_boundary
        
    xmin, xmax, ymin, ymax = bounds.GetEnvelope()

    # Rounded to tile degree boundaries so that we cover the whole data set
    xmin = round_down(xmin, args.degrees_per_tile)
    ymin = round_down(ymin, args.degrees_per_tile)
    xmax = round_up(xmax, args.degrees_per_tile)
    ymax = round_up(ymax, args.degrees_per_tile)

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
            polybox = polygon_for_tile(x, y, args.degrees_per_tile, args.degrees_per_tile)
            if args.skip_boundary or polybox.Intersects(bounds):
                output_filename = os.path.join(args.tile_dir, filename_for_coordinates(x, y, args.degrees_per_tile))
                # If input is in feet, will need to scale tile to meters
                if args.input_units == "feet":
                    scale = 0.3048
                else:
                    scale = 1

                process_args.append((x, y, warped_vrt_filename, output_filename, scale,
                                     args.degrees_per_tile, args.samples_per_tile))
            
            x += args.degrees_per_tile
        y += args.degrees_per_tile

    results = pool.map(process_tile, process_args)
    if any(map(lambda x: isinstance(x, KeyboardInterrupt), results)):
       print('Ctrl-C was entered.')
       exit(1)

    pool.close()
    pool.join()

    # Run prominence and merge_divide_trees
    print("Running prominence")
    prominence_binary = os.path.join(args.binary_dir, "prominence")
    extra_args = ""
    if args.bathymetry:
        extra_args += " -b"
    prom_command = f"{prominence_binary} --v=1 -f CUSTOM-{args.degrees_per_tile}-{args.samples_per_tile}" + \
        f" -i {args.tile_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {args.min_prominence}" + \
        extra_args + \
        f" -- {ymin} {ymax} {xmin} {xmax}"
    run_command(prom_command)

    print("Running merge")
    merge_binary = os.path.join(args.binary_dir, "merge_divide_trees")
    merge_inputs = os.path.join(args.output_dir, "*_pruned_*.dvt")
    merge_output = os.path.join(args.output_dir, "results")
    units_multiplier = 1
    if args.output_units == "feet":
        units_multiplier = 3.28084
    extra_args = ""
    if args.bathymetry:
        extra_args += " -b"
    merge_command = f"{merge_binary} --v=1 -f" + \
        f" -t {args.threads} -m {args.min_prominence} -s {units_multiplier}" + \
        extra_args + \
        f" {merge_output} {merge_inputs}"
    run_command(merge_command)

if __name__ == '__main__':
    main()
