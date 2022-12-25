# Download GLO tiles and run the prominence program on them
#
# Requires these programs to be in the path:
#
# aws (Command-line interface to AWS)
# gdal_translate

import argparse
import os
import subprocess

from multiprocessing import Pool

def run_command(command_string):
    print("> " + command_string)
    retval = subprocess.call(command_string, shell=True)

def process_tile(args):
    (lat, lng, tile_dir) = args
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

        
    filename_base = f"Copernicus_DSM_COG_10_{fname_lat}_00_{fname_lng}_00_DEM"

    flt_filename = os.path.join(tile_dir, filename_base +  ".flt")
    
    # FLT tile already exists locally?
    if not os.path.isfile(flt_filename):
        tif_filename = os.path.join(tile_dir, filename_base + ".tif")
        # Download TIF file if we don't have it yet
        if not os.path.isfile(tif_filename):
            aws_filename = f"s3://copernicus-dem-30m/{filename_base}/{filename_base}.tif"
            aws_command = f"aws s3 cp --no-sign-request {aws_filename} {tile_dir}"
            run_command(aws_command)

        # Only process tile if TIF file is found (some tiles don't exist)
        if os.path.isfile(tif_filename):
            # Convert to .flt file format
            gdal_command = f"gdal_translate -of EHdr {tif_filename} {flt_filename}"
            run_command(gdal_command)

            # Delete unneeded auxiliary files
            for extension in [".flt.aux.xml", ".hdr", ".prj"]:
                filename_to_delete = os.path.join(tile_dir, filename_base + extension)
                if os.path.isfile(filename_to_delete):
                    os.remove(filename_to_delete)
        


def main():
    parser = argparse.ArgumentParser(description='Download GLO tiles and run prominence on them')
    parser.add_argument('--tile_dir', 
                        help="Directory to place input tiles")
    parser.add_argument('--output_dir',
                        help="Directory to place prominence results")
    parser.add_argument('--prominence_command', default='release/prominence',
                        help="Path to prominence binary")
    parser.add_argument('--kml_polygon',
                        help="File with KML polygon to filter input tiles"),
    parser.add_argument('--threads', default=1, type=int,
                        help="Number of threads to use in computing prominence")
    parser.add_argument('--min_prominence', default=100, type=float,
                        help="Min prominence of resultant peaks, in input units")
    parser.add_argument('min_lat', type=int)
    parser.add_argument('max_lat', type=int)
    parser.add_argument('min_lng', type=int)
    parser.add_argument('max_lng', type=int)

    args = parser.parse_args()
    full_tile_dir = os.path.expanduser(args.tile_dir)

    # Run tile downloads in parallel because they're slow
    pool = Pool(args.threads)

    process_args = []
    
    for lat in range(args.min_lat, args.max_lat):
        for lng in range(args.min_lng, args.max_lng):
            process_args.append((lat, lng, full_tile_dir))

    pool.map(process_tile, process_args)
    pool.close()
    pool.join()

    # Run prominence
    prom_command = f"{args.prominence_command} --v=1 -f GLO30 -i {args.tile_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {args.min_prominence}"

    if args.kml_polygon:
        prom_command += f" -k {args.kml_polygon}"
    
    prom_command += f" -- {args.min_lat} {args.max_lat} {args.min_lng} {args.max_lng}"
    run_command(prom_command)

if __name__ == '__main__':
    main()
