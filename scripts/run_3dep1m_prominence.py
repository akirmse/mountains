# Download 3DEP 1m LIDAR tiles and run the prominence program on them
#
# Requires these programs to be in the path:
#
# gdal_translate

import argparse
import os
import requests
import subprocess

from multiprocessing import Pool

# Where to download tiles from
DOWNLOAD_URL_BASE = "https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m/Projects"

def run_command(command_string):
    print("> " + command_string)
    retval = subprocess.call(command_string, shell=True)

def process_tile(args):
    (project, x, y, zone, tile_dir) = args
        
    filename_base = f"USGS_1M_{zone}_x{x:02}y{y:03}"

    flt_filename = os.path.join(tile_dir, filename_base +  ".flt")
    
    # Tile already exists locally?
    if not os.path.isfile(flt_filename):
        tif_filename = os.path.join(tile_dir, filename_base + ".tif")

        # Download TIF file
        tif_url = f"{DOWNLOAD_URL_BASE}/{project}/TIFF/{filename_base}_{project}.tif"
        print("Downloading tile for x =", x, ", y =", y)
        r = requests.get(tif_url, allow_redirects=True)
        if r.status_code == 200:
            with open(tif_filename, "wb") as f:
                f.write(r.content)

            # Convert to .flt file format
            gdal_command = f"gdal_translate -of EHdr {tif_filename} {flt_filename}"
            run_command(gdal_command)

def main():
    parser = argparse.ArgumentParser(description='Download GLO tiles and run prominence on them')
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument('--tile_dir', 
                               help="Directory to place input tiles",
                               required=True)
    requiredNamed.add_argument('--output_dir',
                               help="Directory to place prominence results",
                               required=True)
    requiredNamed.add_argument('--project', 
                               help="Name of LIDAR project",
                               required=True)
    requiredNamed.add_argument('--zone', type=int,
                               help="UTM zone of tiles",
                               required=True)
    parser.add_argument('--prominence_command', default='release/prominence',
                        help="Path to prominence binary")
    parser.add_argument('--threads', default=1, type=int,
                        help="Number of threads to use in computing prominence")
    parser.add_argument('min_x', type=int)
    parser.add_argument('max_x', type=int)
    parser.add_argument('min_y', type=int)
    parser.add_argument('max_y', type=int)

    args = parser.parse_args()
    full_tile_dir = os.path.expanduser(args.tile_dir)

    # Run tile downloads in parallel because they're slow
    pool = Pool(args.threads)

    process_args = []
    
    for x in range(args.min_x, args.max_x):
        for y in range(args.min_y, args.max_y):
            process_args.append((args.project, x, y, args.zone, full_tile_dir))

    pool.map(process_tile, process_args)
    pool.close()
    pool.join()

    # Run prominence
    prom_command = f"{args.prominence_command} --v=1 -f 3DEP-1M -i {args.tile_dir} -o {args.output_dir} " + \
        f"-t {args.threads} -z {args.zone}" + \
        f" -- {args.min_y} {args.max_y} {args.min_x} {args.max_x}"
    run_command(prom_command)

if __name__ == '__main__':
    main()
