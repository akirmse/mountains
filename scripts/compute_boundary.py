# Compute boundary shapefile of a set of given input rasters

import argparse
import glob
import math
import os
import signal
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed

from osgeo import gdal, ogr, osr

from boundary import Boundary, process_chunk


def process_files(files, threads):
    print("  Computing boundary")

    if threads <= 1:
        b = Boundary()
        for f in files:
            print(f)
            b.add_dataset(f)
        b.write_to_file('boundary.shp')
        return

    # Split files into chunks of batch_size so each thread does a balanced amount
    # of both I/O and Union work independently, mirroring Boundary's own batching.
    chunk_size = Boundary.batch_size
    chunks = [files[i:i + chunk_size] for i in range(0, len(files), chunk_size)]

    # Submit all chunks; merge each chunk's boundary into the final result as it
    # completes rather than waiting for all threads to finish first.
    boundary = Boundary()
    with ThreadPoolExecutor(max_workers=threads) as executor:
        futures = [executor.submit(process_chunk, chunk) for chunk in chunks]
        for future in as_completed(futures):
            geom = future.result()
            if not geom.IsEmpty():
                boundary.add_geometry(geom)

    boundary.write_to_file('boundary.shp')


def main():
    parser = argparse.ArgumentParser(description='Convert LIDAR to standard tiles')
    parser.add_argument('input_files', type=str, nargs='+',
                        help='Input Lidar tiles, or GDAL VRT of tiles')
    parser.add_argument('--threads', type=int, default=1,
                        help='Number of threads for parallel boundary computation')
    args = parser.parse_args()

    gdal.UseExceptions()

    # Treat each input as potentially a glob, and then flatten the list
    input_files = []
    for filespec in args.input_files:
        files = glob.glob(filespec)
        if len(files) == 0:
            print(f"Input filespec {filespec} matched no files; exiting.")
            exit(1)
        input_files.extend(files)

    process_files(input_files, args.threads)


if __name__ == '__main__':
    main()
