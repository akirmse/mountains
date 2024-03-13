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

#include "isolation_task.h"
#include "coordinate_system.h"
#include "isolation_finder.h"
#include "isolation_results.h"
#include "peak_finder.h"

#include "easylogging++.h"

#include <stdio.h>
#include <memory>
#include <set>

using std::set;
using std::string;
using std::vector;

// bounds is an array of min_lat, max_lat, min_lng, max_lng
IsolationTask::IsolationTask(TileCache *cache, const string &output_dir,
                             double bounds[], double minIsolationKm) {
  mCache = cache;
  mOutputDir = output_dir;
  mBounds = bounds;
  mMinIsolationKm = minIsolationKm;
}

bool IsolationTask::run(double lat, double lng, const CoordinateSystem &coordinateSystem, const FileFormat format) {
  // Load the main tile manually; cache could delete it if we allow it to be cached
  std::unique_ptr<Tile> tile(mCache->loadWithoutCaching(lat, lng, coordinateSystem));
  if (tile.get() == nullptr) {
    VLOG(2) << "Couldn't load tile for " << lat << " " << lng;
    return false;
  }

  //
  // Find peaks in this tile
  //
  
  PeakFinder pfinder(tile.get());
  vector<Offsets> peaks = pfinder.findPeaks();

  //
  // Compute isolation of each peak
  //
  
  IsolationFinder ifinder(mCache, tile.get(), coordinateSystem, format);

  IsolationResults results;

  auto minLat = mBounds[0];
  auto maxLat = mBounds[1];
  auto minLng = mBounds[2];
  auto maxLng = mBounds[3];
  
  printf("Processing tile %.1f %.1f\n", lat, lng);

  VLOG(1) << "Found " << peaks.size() << " peaks";

  for (auto offset : peaks) {
    LatLng peak = coordinateSystem.getLatLng(offset);
    // Discard any peaks outside requested bounds
    if (peak.latitude() < minLat || peak.latitude() > maxLat ||
        peak.longitude() < minLng || peak.longitude() > maxLng) {
      continue;
    }
    
    IsolationRecord record = ifinder.findIsolation(offset);

    LatLng higher = record.closestHigherGround;
    if (record.foundHigherGround) {
      VLOG(2) << "Higher ground for " << peak.latitude() << " " << peak.longitude()
              << " at " << higher.latitude() << " " << higher.longitude();
      auto distance = peak.distanceEllipsoid(higher) / 1000;  // kilometers

      if (distance > mMinIsolationKm) {
        results.addResult(peak, tile->get(offset), higher, distance);
      } else {
        VLOG(3) << "Isolation < minimum: " << distance;
      }
    } else {
      // -1 isolation to indicate no higher peak found
      results.addResult(peak, tile->get(offset), higher, -1);
    }
  }

  return results.save(mOutputDir, lat, lng);
}
