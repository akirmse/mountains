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

#include "coordinate_system.h"
#include "isolation_task.h"
#include "ThreadPool.h"
#include "tile.h"
#include "tile_loading_policy.h"

#include "easylogging++.h"
#include "getopt_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <cmath>
#include <set>

using std::ceil;
using std::floor;
using std::set;
using std::string;
using std::vector;

INITIALIZE_EASYLOGGINGPP

static void usage() {
  printf("Usage:\n");
  printf("  isolation min_lat max_lat min_lng max_lng\n");
  printf("  where coordinates are integer degrees\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -i directory     Directory with terrain data\n");
  printf("  -m min_isolation Minimum isolation threshold for output, default = 1km\n");
  printf("  -o directory     Directory for output data\n");
  printf("  -t num_threads   Number of threads, default = 1\n");
  exit(1);
}

int main(int argc, char **argv) {
  string terrain_directory(".");
  string output_directory(".");

  float minIsolation = 1;
  int numThreads = 1;
  
  // Parse options
  START_EASYLOGGINGPP(argc, argv);
  int ch;
  // Swallow --v that's parsed by the easylogging library
  const struct option long_options[] = {
    {"v", required_argument, nullptr, 0},
    {nullptr, 0, 0, 0},
  };
  while ((ch = getopt_long(argc, argv, "i:m:o:t:", long_options, nullptr)) != -1) {
    switch (ch) {
    case 'i':
      terrain_directory = optarg;
      break;

    case 'm':
      minIsolation = (float) atof(optarg);
      break;

    case 'o':
      output_directory = optarg;
      break;

    case 't':
      numThreads = atoi(optarg);
      break;
    }
  }

  argc -= optind;
  argv += optind;

  if (argc < 4) {
    usage();
  }

  double bounds[4];
  for (int i = 0; i < 4; ++i) {
    char *endptr;
    bounds[i] = strtod(argv[i], &endptr);
    if (*endptr != 0) {
      printf("Couldn't parse argument %d as number: %s\n", i + 1, argv[i]);
      usage();
    }
  }

  // TODO: Maybe support other file formats in the future?
  FileFormat fileFormat(FileFormat::Value::HGT);
  BasicTileLoadingPolicy policy(terrain_directory, fileFormat);
  const int CACHE_SIZE = 50;
  auto cache = std::make_unique<TileCache>(&policy, CACHE_SIZE);

  VLOG(2) << "Using " << numThreads << " threads";
  
  auto threadPool = std::make_unique<ThreadPool>(numThreads);
  int num_tiles_processed = 0;
  vector<std::future<bool>> results;
  for (auto lat = (float) floor(bounds[0]); lat < ceil(bounds[1]); lat += 1) {
     for (auto lng = (float) floor(bounds[2]); lng < ceil(bounds[3]); lng += 1) {
       std::shared_ptr<CoordinateSystem> coordinateSystem(
         fileFormat.coordinateSystemForOrigin(lat, lng));
       
      IsolationTask *task = new IsolationTask(
        cache.get(), output_directory, bounds, minIsolation);
      results.push_back(threadPool->enqueue([=] {
            return task->run(lat, lng, *coordinateSystem, fileFormat);
          }));
    }
  }

  for (auto && result : results) {
    if (result.get()) {
      num_tiles_processed += 1;
    }
  }
    
  printf("Tiles processed = %d\n", num_tiles_processed);

  return 0;
}
