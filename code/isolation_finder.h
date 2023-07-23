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


#ifndef _ISOLATION_FINDER_H_
#define _ISOLATION_FINDER_H_

#include <memory>
#include <vector>
#include "coordinate_system.h"
#include "latlng.h"
#include "tile_cache.h"
#include "file_format.h"

struct IsolationRecord {
  bool foundHigherGround;
  LatLng closestHigherGround;
  float distance;  // distance to peak in meters

  IsolationRecord()
      : foundHigherGround(false),
        closestHigherGround(0, 0),
        distance(0) {
  }

  IsolationRecord(const IsolationRecord &other)
      : foundHigherGround(other.foundHigherGround),
        closestHigherGround(other.closestHigherGround),
        distance(other.distance) {
  }

  void operator=(const IsolationRecord &other) {
    foundHigherGround = other.foundHigherGround;
    closestHigherGround = other.closestHigherGround;
    distance = other.distance;
  }
};

class IsolationFinder {
public:
  IsolationFinder(TileCache *cache, const Tile *tile,
                  const CoordinateSystem &coordinateSystem, FileFormat format);
  
  IsolationRecord findIsolation(Offsets peak) const;
  
private:
  
  const Tile *mTile;
  TileCache *mCache;
  std::unique_ptr<CoordinateSystem> mCoordinateSystem;
  FileFormat mFormat;

  // Search tile for a point higher than seedElevation.
  //
  // If peakLocation is nullptr, then seedPoint is inside this tile and seedPoint gives its location.
  // If peakLocation is non-null, then peakLocation is outside the tile, and seedPoint is the closest point
  // in the tile to peakLocation.
  IsolationRecord findIsolation(const Tile *tile, const CoordinateSystem *tileCoordinateSystem, const LatLng *peakLocation, Offsets seedPoint, Elevation seedElevation) const;
  
  // Check the neighboring tile with the given lat/lng, where seedCoords give the closest point
  // in the neighboring tile to the peak, and elev is the height of the peak.
  // peakLocation has the same meaning as in findIsolation
  IsolationRecord checkNeighboringTile(float lat, float lng, const LatLng *peakLocation,
                                       Offsets seedCoords, Elevation elev) const;
};

#endif  // _ISOLATION_FINDER_H_
