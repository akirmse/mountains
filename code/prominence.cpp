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

#include "filter.h"
#include "peakbagger_collection.h"
#include "point_map.h"
#include "prominence_task.h"
#include "ThreadPool.h"
#include "tile.h"
#include "tile_cache.h"
#include "tile_loading_policy.h"

#include "easylogging++.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif
#ifdef PLATFORM_WINDOWS
#include "getopt-win.h"
#endif
#include <cassert>
#include <cmath>

using std::ceil;
using std::floor;
using std::string;
using std::vector;

INITIALIZE_EASYLOGGINGPP

static const int NO_UTM_ZONE = -1;

static void usage() {
  printf("Usage:\n");
  printf("  prominence [options] min_lat max_lat min_lng max_lng\n");
  printf("  where coordinates are integer degrees\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -i directory      Directory with terrain data\n");
  printf("  -o directory      Directory for output data\n");
  printf("  -f format         \"SRTM\", \"NED13-ZIP\", \"NED1-ZIP\", \"NED19\", \"3DEP-1M\", \"GLO30\" input files\n");
  printf("  -k filename       File with KML polygon to filter input tiles\n");
  printf("  -m min_prominence Minimum prominence threshold for output, default = 300ft\n");
  printf("  -p filename       Peakbagger peak database file for matching\n");
  printf("  -t num_threads    Number of threads, default = 1\n");
  printf("  -a                Compute anti-prominence instead of prominence\n");
  exit(1);
}

int main(int argc, char **argv) {
  string terrain_directory(".");
  string output_directory(".");
  string peakbagger_filename;
  string polygonFilename;

  int minProminence = 300;
  int numThreads = 1;
  FileFormat fileFormat = FileFormat(FileFormat::Value::HGT);

  // Parse options
  START_EASYLOGGINGPP(argc, argv);
  int ch;
  string str;
  bool antiprominence = false;
  int utmZone = NO_UTM_ZONE;
  while ((ch = getopt(argc, argv, "af:i:k:m:o:p:t:z:")) != -1) {
    switch (ch) {
    case 'a':
      antiprominence = true;
      break;
      
    case 'f': {
      auto format = FileFormat::fromName(optarg);
      if (format == nullptr) {
        printf("Unknown file format %s\n", optarg);
        usage();
      }

      fileFormat = *format;
      break;
    }

    case 'i':
      terrain_directory = optarg;
      break;

    case 'k':
      polygonFilename = optarg;
      break;

    case 'm':
      minProminence = atoi(optarg);
      break;

    case 'o':
      output_directory = optarg;
      break;

    case 'p':
      peakbagger_filename = optarg;
      break;

    case 't':
      numThreads = atoi(optarg);
      break;

    case 'z':
      utmZone = atoi(optarg);
      assert((utmZone > 0 && utmZone <= 60) && "UTM zone out of range");
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 4) {
    usage();
  }

  if (fileFormat.isUtm() && utmZone == NO_UTM_ZONE) {
    printf("You must specify a UTM zone with this format");
    exit(1);
  }

  // Load Peakbagger database?
  PeakbaggerCollection pb_collection;
  PointMap *peakbagger_peaks = new PointMap();
  if (!peakbagger_filename.empty()) {
    printf("Loading peakbagger database\n");
    if (!pb_collection.Load(peakbagger_filename)) {
      printf("Couldn't load peakbagger database from %s\n", peakbagger_filename.c_str());
      exit(1);
    }

    // Put Peakbagger peaks in spatial structure
    const vector<PeakbaggerPoint> &pb_peaks = pb_collection.points();
    for (int i = 0; i < (int) pb_peaks.size(); ++i) {
      peakbagger_peaks->insert(&pb_peaks[i]);
    }
  }
  
  float bounds[4];
  for (int i = 0; i < 4; ++i) {
    char *endptr;
    bounds[i] = strtof(argv[i], &endptr);
    if (*endptr != 0) {
      printf("Couldn't parse argument %d as number: %s\n", i + 1, argv[i]);
      usage();
    }
  }

  // Load filtering polygon
  Filter filter;
  if (!polygonFilename.empty()) {
    if (!filter.addPolygonsFromKml(polygonFilename)) {
      printf("Couldn't load KML polygon from %s\n",polygonFilename.c_str());
      exit(1);
    }
  }

  // Caching doesn't do anything for our calculation and the tiles are huge
  BasicTileLoadingPolicy policy(terrain_directory, fileFormat);
  policy.enableNeighborEdgeLoading(true);
  if (utmZone != NO_UTM_ZONE) {
    policy.setUtmZone(utmZone);
  }
  const int CACHE_SIZE = 2;
  TileCache *cache = new TileCache(&policy, peakbagger_peaks, CACHE_SIZE);
  
  VLOG(2) << "Using " << numThreads << " threads";
  
  ThreadPool *threadPool = new ThreadPool(numThreads);
  int num_tiles_processed = 0;
  vector<std::future<bool>> results;
  float lat = bounds[0];
  while (lat < bounds[1]) {
    float lng = bounds[2];
    while (lng < bounds[3]) {
      // Allow specifying longitude ranges that span the antimeridian (lng > 180)
      auto wrappedLng = lng;
      if (wrappedLng >= 180) {
        wrappedLng -= 360;
      }

      // Skip tiles that don't intersect filtering polygon
      if (!filter.intersects(lat, lat + fileFormat.degreesAcross(),
                             lng, lng + fileFormat.degreesAcross())) {
        VLOG(3) << "Skipping tile that doesn't intersect polygon " << lat << " " << lng;
      } else {
        // Actually calculate prominence
        ProminenceTask *task = new ProminenceTask(
          cache, output_directory, minProminence);
        task->setAntiprominence(antiprominence);
        results.push_back(threadPool->enqueue([=] {
              return task->run(lat, wrappedLng);
            }));
      }

      lng += fileFormat.degreesAcross();
    }
    lat += fileFormat.degreesAcross();
  }

  for (auto && result : results) {
    if (result.get()) {
      num_tiles_processed += 1;
    }
  }
    
  printf("Tiles processed = %d\n", num_tiles_processed);

  delete threadPool;
  delete cache;
  delete peakbagger_peaks;

  return 0;
}
