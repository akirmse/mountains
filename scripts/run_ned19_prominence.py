# Download NED 1/9 arcsecond tiles and run the prominence program on them
#
# Requires these programs to be in the path:
#
# gdal_translate

import argparse
import os
import re
import requests
import signal
import subprocess

from interrupt import handle_ctrl_c, init_pool
from multiprocessing import Pool
from zipfile import ZipFile

# Lists all tiles by name
CONTENTS_URL="https://thor-f5.er.usgs.gov/ngtoc/metadata/waf/elevation/1-9_arc-second/img/undefined/"

# Where to download tiles once their name is known
DOWNLOAD_URL_BASE="https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/19/IMG/"


def run_command(command_string):
    print("> " + command_string)
    retval = subprocess.call(command_string, shell=True)

def fractionalDegree(degree: float) -> int:
    """Return fractional hundredths of a degree in given value"""
    excess = abs(degree - int(degree))
    return int(100 * excess)
    
@handle_ctrl_c
def process_tile(args):
    (lat, lng, tile_dir, contents) = args
    lat_str = "%02dx%02d" % (abs(int(lat)), fractionalDegree(lat))
    lng_str = "%03dx%02d" % (abs(int(lng)), fractionalDegree(lng))

    if lat >= 0:
        fname_lat = f"n{lat_str}"
    else:
        fname_lat = f"s{lat_str}"

    if lng >= 0:
        fname_lng = f"e{lng_str}"
    else:
        fname_lng = f"w{lng_str}"

    filename_base = f"ned19_{fname_lat}_{fname_lng}"

    flt_filename = os.path.join(tile_dir, filename_base +  ".flt")

    # Tile already exists locally?
    if not os.path.isfile(flt_filename):
        # Find full name of source file
        match = re.search(f"\"({filename_base}.*)_meta.xml\"", contents)
        if match is None or len(match.groups()) < 1:
            print(f"No tile for {filename_base}")
            return

        full_filename = match.group(1)

        # Download zip file
        zip_url = f"{DOWNLOAD_URL_BASE}{full_filename}.zip"
        r = requests.get(zip_url, allow_redirects=True)
        zip_filename = os.path.join(tile_dir, f"{full_filename}.zip")
        with open(zip_filename, "wb") as f:
            f.write(r.content)

        # Decompress zip file
        with ZipFile(zip_filename, "r") as zip_ref:
            zip_ref.extractall(tile_dir)
        
        # Convert to .flt file format
        source_filename = os.path.join(tile_dir, full_filename + ".img")
        gdal_command = f"gdal_translate -of EHdr {source_filename} {flt_filename}"
        run_command(gdal_command)

def main():
    parser = argparse.ArgumentParser(description='Download NED 1/9 arcsecond tiles and run prominence on them')
    requiredNamed = parser.add_argument_group('required named arguments')
    requiredNamed.add_argument('--tile_dir', 
                               help="Directory to place input tiles", required=True)
    requiredNamed.add_argument('--output_dir',
                               help="Directory to place prominence results", required=True)
    parser.add_argument('--prominence_command', default='release/prominence',
                        help="Path to prominence binary")
    parser.add_argument('--threads', default=1, type=int,
                        help="Number of threads to use in computing prominence")
    parser.add_argument('--min_prominence', default=100, type=float,
                        help="Min prominence of resultant peaks, in input units")
    parser.add_argument('min_lat', type=float)
    parser.add_argument('max_lat', type=float)
    parser.add_argument('min_lng', type=float)
    parser.add_argument('max_lng', type=float)

    args = parser.parse_args()
    full_tile_dir = os.path.expanduser(args.tile_dir)

    # Filenames aren't deterministic; need to get list of possibilities from
    # a table of contents directory listing
    contents = requests.get(CONTENTS_URL).content.decode("utf-8")

    # Run tile downloads in parallel because they're slow
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    pool = Pool(args.threads, initializer=init_pool)

    process_args = []

    # Size of tile in degrees.  NED filenames refer to NW corner of tile,
    # so we have to go this far south to match the input rectangle on the Earth.
    tile_size = 0.25  
    lat = args.min_lat + tile_size
    while lat < args.max_lat + tile_size:
        lng = args.min_lng
        while lng < args.max_lng:
            process_args.append((lat, lng, full_tile_dir, contents))
            lng += tile_size
        lat += tile_size

    results = pool.map(process_tile, process_args)
    if any(map(lambda x: isinstance(x, KeyboardInterrupt), results)):
       print('Ctrl-C was entered.')
       exit(1)

    pool.close()
    pool.join()

    # Run prominence
    prom_command = f"{args.prominence_command} --v=1 -f NED19 -i {args.tile_dir} -o {args.output_dir}" + \
        f" -t {args.threads} -m {args.min_prominence}" + \
        f" -- {args.min_lat} {args.max_lat} {args.min_lng} {args.max_lng}"
    run_command(prom_command)

if __name__ == '__main__':
    main()
