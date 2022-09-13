/*
 * MIT License
 * 
 * Copyright (c) 2017 Andrew Kirmse
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Filters a text file of lat, lngs by a polygon specified in a KML file.

#include "easylogging++.h"
#include "filter.h"
#include "getopt.h"
#include "latlng.h"
#include "util.h"

#include <fstream>
#include <string>
#include <vector>

using std::string;
using std::vector;

INITIALIZE_EASYLOGGINGPP

static void usage() {
  printf("Usage:\n");
  printf("  filter_points input_file polygon_file output_file\n");
  printf("  Filters input_file, printing only lines inside the polygon in polygon_file to output_file\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -a longitude    Add 360 to any longitudes < this value in polygon.\n");
  printf("                  Allows polygon to cross antimeridian (negative longitude gets +360)\n");
  printf("\n");
  exit(1);
}

int main(int argc, char **argv) {
  float wrapLongitude = -180;
  
  int ch;
  string str;
  while ((ch = getopt(argc, argv, "a:")) != -1) {
    switch (ch) {
    case 'a':
      wrapLongitude = static_cast<float>(atof(optarg));
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 3) {
    usage();
  }

  string inputFilename = argv[0];
  string polygonFilename = argv[1];
  string outputFilename = argv[2];

  // Read polygon
  Filter filter;
  if (!filter.addPolygonsFromKml(polygonFilename)) {
    exit(1);
  }

  // Wrap around antimeridian?
  filter.setWrapLongitude(wrapLongitude);
  
  if (!fileExists(inputFilename)) {
    printf("Can't find input file %s\n", inputFilename.c_str());
    exit(1);
  }

  std::ifstream inputFile(inputFilename);
  std::ofstream outputFile(outputFilename);
  
  string line;
  vector<string> elements;
  int numPointsInPolygon = 0;
  int numPointsNotInPolygon = 0;
  while (inputFile.good()) {
    std::getline(inputFile, line);

    // Skip blank lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    split(line, ',', elements);
    float lat = stof(elements[0]);
    float lng = stof(elements[1]);
    LatLng point(lat, lng);

    if (filter.isPointInside(point)) {
      printf("Point %f %f is in polygon\n", lat, lng);
      numPointsInPolygon += 1;

      outputFile << line << std::endl;
    } else {
      printf("Point %f %f is not in polygon\n", lat, lng);
      numPointsNotInPolygon += 1;
    }
  }

  outputFile.close();
  printf("Found %d points in polygon, %d points outside\n",
         numPointsInPolygon, numPointsNotInPolygon);
}
