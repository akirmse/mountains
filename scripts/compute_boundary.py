# Compute boundary shapefile of a set of given input rasters

import argparse
import glob
import math
import os
import signal
import subprocess

from osgeo import gdal, ogr, osr

from boundary import Boundary

def process_files(files):
    print("  Computing boundary")

    # With many thousands of files, continually merging into one boundary
    # gets slow.  So instead we do it in batches.
    boundary = Boundary()
    for f in files:
        print(f)
        boundary.add_dataset(f)

    # Write merged geometry
    boundary.write_to_file('boundary.shp')

def main():
    parser = argparse.ArgumentParser(description='Convert LIDAR to standard tiles')
    parser.add_argument('input_files', type=str, nargs='+',
                        help='Input Lidar tiles, or GDAL VRT of tiles')
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

    process_files(input_files)
        
if __name__ == '__main__':
    main()
