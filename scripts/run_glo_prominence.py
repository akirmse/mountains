# Download GLO30 tiles and run the prominence program on them.
# Also supports FABDEM, which is a variant of GLO30.
#
# Requires these programs to be in the path:
#
# aws (Command-line interface to AWS)
# gdal_translate

import argparse
import os
import subprocess

from filter import Filter

from multiprocessing import Pool

def run_command(command_string):
    print("> " + command_string)
    retval = subprocess.call(command_string, shell=True)

def process_tile(args):
    (lat, lng, tile_dir, intermediate_dir, tile_format, download_tiles) = args
    lat_str = "%02d" % abs(lat)
    lng_str = "%03d" % abs(lng)

    if lat >= 0:
        fname_lat = f"N{lat_str}"
    else:
        fname_lat = f"S{lat_str}"

    if lng >= 0:
        fname_lng = f"E{lng_str}"
    else:
        fname_lng = f"W{lng_str}"


    if tile_format == "GLO30":
        filename_base = f"Copernicus_DSM_COG_10_{fname_lat}_00_{fname_lng}_00_DEM"
    elif tile_format == "FABDEM":
        filename_base = f"{fname_lat}{fname_lng}_FABDEM_V1-0"
    else:
        assert False, f"Unknown tile format {tile_format}"

    flt_filename = os.path.join(intermediate_dir, filename_base +  ".flt")
    
    # FLT tile already exists locally?
    if not os.path.isfile(flt_filename):
        tif_filename = os.path.join(tile_dir, filename_base + ".tif")
        # Download TIF file if we don't have it yet?
        if download_tiles and not os.path.isfile(tif_filename):
            aws_filename = f"s3://copernicus-dem-30m/{filename_base}/{filename_base}.tif"
            aws_command = f"aws s3 cp --no-sign-request {aws_filename} {tile_dir}"
            run_command(aws_command)

        # Only process tile if TIF file is found (some tiles don't exist)
        if os.path.isfile(tif_filename):
            # Convert to .flt file format
            gdal_command = f"gdal_translate -of EHdr -ot Float32 {tif_filename} {flt_filename}"
            run_command(gdal_command)

            # Delete unneeded auxiliary files
            for extension in [".flt.aux.xml", ".hdr", ".prj"]:
                filename_to_delete = os.path.join(intermediate_dir, filename_base + extension)
                if os.path.isfile(filename_to_delete):
                    os.remove(filename_to_delete)
        


def main():
    parser = argparse.ArgumentParser(description='Download GLO tiles and run prominence on them')
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument('--tile_dir', required = True,
                              help="Directory to place source tiles")
    requiredNamed.add_argument('--output_dir', required = True,
                               help="Directory to place prominence results")
    parser.add_argument('--intermediate_dir',
                        help="Directory to place unzipped FLT tiles; default=tile_dir")
    parser.add_argument('--download_tiles', dest='download_tiles', action='store_true',
                        help="Download any missing tiles")
    parser.add_argument('--no_download_tiles', dest='download_tiles', action='store_false',
                        help="Don't download any missing tiles")
    parser.set_defaults(download_tiles=True)

    parser.add_argument('--prominence_command', default='release/prominence',
                        help="Path to prominence binary")
    parser.add_argument('--kml_polygon',
                        help="File with KML polygon to filter input tiles"),
    parser.add_argument('--threads', default=1, type=int,
                        help="Number of threads to use in computing prominence")
    parser.add_argument('--min_prominence', default=100, type=float,
                        help="Min prominence of resultant peaks, in input units")
    parser.add_argument('--format', default="GLO30", type=str, choices={"GLO30", "FABDEM"},
                        help="Format of the input tiles")
    parser.add_argument('min_lat', type=int)
    parser.add_argument('max_lat', type=int)
    parser.add_argument('min_lng', type=int)
    parser.add_argument('max_lng', type=int)

    args = parser.parse_args()
    full_tile_dir = os.path.expanduser(args.tile_dir)
    intermediate_dir = args.intermediate_dir or full_tile_dir

    # Set up filtering polygon
    filterPolygon = Filter()
    if args.kml_polygon:
        filterPolygon.addPolygonsFromKml(args.kml_polygon)

    # Run tile downloads in parallel because they're slow
    pool = Pool(args.threads)

    process_args = []
    
    for lat in range(args.min_lat, args.max_lat):
        for lng in range(args.min_lng, args.max_lng):
            # Allow specifying longitude ranges that span the antimeridian (lng > 180)
            wrappedLng = lng;
            if wrappedLng >= 180:
                wrappedLng -= 360

            # Skip tiles that don't intersect filtering polygon
            if filterPolygon.intersects(lat, lat + 1, lng, lng + 1):
                process_args.append((lat, wrappedLng, full_tile_dir, intermediate_dir,
                                     args.format, args.download_tiles))

    pool.map(process_tile, process_args)
    pool.close()
    pool.join()

    # Run prominence
    prom_command = f"{args.prominence_command} --v=1 -f {args.format}" + \
        f" -i {args.intermediate_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {args.min_prominence}"

    if args.kml_polygon:
        prom_command += f" -k {args.kml_polygon}"
    
    prom_command += f" -- {args.min_lat} {args.max_lat} {args.min_lng} {args.max_lng}"
    run_command(prom_command)

if __name__ == '__main__':
    main()
