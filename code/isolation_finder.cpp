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

#include "isolation_finder.h"
#include "coordinate_system.h"
#include "easylogging++.h"
#include "file_format.h"
#include "math_util.h"

#include <assert.h>
#include <stdlib.h>

#include <unordered_set>

using std::vector;

IsolationFinder::IsolationFinder(TileCache *cache, const Tile *tile,
                                 const CoordinateSystem &coordinateSystem, FileFormat format) {
  mTile = tile;
  mCache = cache;
  mCoordinateSystem = std::unique_ptr<CoordinateSystem>(coordinateSystem.clone());
  mFormat = format;
}

IsolationRecord IsolationFinder::findIsolation(Offsets peak) const {
  Elevation elev = mTile->get(peak);
  LatLng peakLocation(mCoordinateSystem->getLatLng(peak));

  // Get minimum lat/lng of tile
  auto originOffsets = Offsets(0, mTile->height() - 1);
  auto origin = mCoordinateSystem->getLatLng(originOffsets);
  int peakLat = static_cast<int>(origin.latitude());
  int peakLng = static_cast<int>(origin.longitude());

  VLOG(2) << "Considering peak at " << peak.x() << " " << peak.y()
          << " lat/lng " << peakLocation.latitude() << " " << peakLocation.longitude() << " "
          << "with elevation " << elev;

  IsolationRecord record;

  // Even if we found higher ground in this tile, there could be closer higher ground in
  // a neighboring tile, or even several tiles away (due to the latitude squish).
  // Initialize with assumption that we'll have to check the whole world
  int bottomLat, topLat, leftLng, rightLng;
  bottomLat = -90;
  topLat = 89;
  leftLng = -180;
  rightLng = 179;
  record.distance = 1e20f;

  // Set of all tiles we've already checked
  std::unordered_set<Offsets::Value> checkedTiles;
  bool hasHigherGroundBeenFound = false;
  
  // Check in concentric rings
  for (int ring = 0; ring < 360; ++ring) {
    int miny = peakLat - ring;
    int maxy = peakLat + ring;
    int minx = peakLng - ring;
    int maxx = peakLng + ring;
    
    minx = std::max(minx, leftLng);
    maxx = std::min(maxx, rightLng);
    miny = std::max(miny, bottomLat);
    maxy = std::min(maxy, topLat);

    for (int lat = miny; lat <= maxy; ++lat) {
      for (int lng = minx; lng <= maxx; ++lng) {
        // Deal with antimeridian: bring longitude back into range
        int neighborLng = lng;
        if (neighborLng >= 180) {
          neighborLng -= 360;
        }
        if (neighborLng < -180) {
          neighborLng += 360;
        }
        
        // Skip any tile already checked
        auto tileIndex = Offsets(lat, neighborLng).value();
        if (checkedTiles.find(tileIndex) != checkedTiles.end()) {
          continue;
        }
        checkedTiles.insert(tileIndex);

        // Find closest point in tile (on edge or corner if it's in a neighbor)
        int seedy = peak.y();
        if (lat < peakLat) {
          seedy = 0;
        } else if (lat > peakLat) {
          seedy = mTile->height() - 1;
        }
        
        int seedx = peak.x();
        if (lng < peakLng) {
          seedx = mTile->width() - 1;
        } else if (lng > peakLng) {
          seedx = 0;
        }

        // We only want to do the slow, exact check if we're not in the peak's tile
        LatLng *locationToUse = nullptr;
        if (lat != peakLat || lng != peakLng) {
          locationToUse = &peakLocation;
        }
        IsolationRecord neighborRecord = checkNeighboringTile(
          static_cast<float>(lat), static_cast<float>(neighborLng), locationToUse,
          Offsets(seedx, seedy), elev);
        // Distance in record is distance to seed; we want distance to peak
        if (neighborRecord.foundHigherGround) {
          neighborRecord.distance = peakLocation.distance(neighborRecord.closestHigherGround);
          VLOG(2) << "Found higher ground at " << neighborRecord.closestHigherGround.latitude()
                  << " " << neighborRecord.closestHigherGround.longitude()
                  << " distance = " << neighborRecord.distance;
          if (neighborRecord.distance < record.distance) {
            record = neighborRecord;
          }
        }
      }
    }

    // Found any higher ground in this ring?
    if (record.foundHigherGround && !hasHigherGroundBeenFound) {
      // Find bounding box that encloses all samples as close as nearest higher ground so far
      LatLng peakPoint(peakLocation.latitude(), peakLocation.longitude());
      vector<LatLng> corners = peakPoint.GetBoundingBoxForCap(record.distance);
      
      // Rounds towards minus infinity (tiles named by lower left corner)
      bottomLat = static_cast<int>(floor(corners[0].latitude()));
      topLat = static_cast<int>(floor(corners[1].latitude()));
      leftLng = static_cast<int>(floor(corners[0].longitude()));
      rightLng = static_cast<int>(floor(corners[1].longitude()));
      // Deal with antimeridian
      if (leftLng > rightLng) {
        // Right edge will now be > 180
        rightLng += 360;
        // Put peak latitude between leftLng and rightLng
        if (peakLng < 0) {
          peakLng += 360;
        }
      }
      hasHigherGroundBeenFound = true;
      VLOG(2) << "Found higher ground; range to check is now "  << bottomLat << " "
              << topLat << " " << leftLng << " " << rightLng;
    }
    
    // Done if we're past all the bounds to check
    if (minx <= leftLng && maxx >= rightLng &&
        miny <= bottomLat && maxy >= topLat) {
      VLOG(2) << "Exiting ring loop #" << ring << " with " << miny << " " << maxy
              << " " << minx << " " << maxx;
      break;
    }
  }

  return record;
}

IsolationRecord IsolationFinder::findIsolation(const Tile *tile, const CoordinateSystem *tileCoordinateSystem,
                                               const LatLng *peakLocation, Offsets seedPoint, Elevation seedElevation) const {
  IsolationRecord record;
  
  // Exit immediately if seedElevation >= our max.
  if (seedElevation >= tile->maxElevation()) {
    return record;
  }

  // An array with one entry per row of the tile.
  // Each entry is a scale factor in [0, 1] that should be multiplied by
  // any distance in the longitude (x) direction.  This compensates for
  // lines of longitude getting closer as latitude increases.  The value
  // of the factor is the cosine of the latitude of the row.
  float *lngDistanceScaleForRow = new float[ tile->height()];
  for (int y = 0; y < tile->height(); ++y) {
    LatLng point = tileCoordinateSystem->getLatLng(Offsets(0, y));
    lngDistanceScaleForRow[y] = cosf(degToRad(point.latitude()));
  }

  // We want to search in concentric circles around the seed point.
  // For simplicity we search in concentric rectangles, but that
  // introduces two other complications:
  //
  // 1) Squares in the tile are not squares in physical distance due
  //    to longitude lines getting closer together as latitude
  //    increases.  So we need to consider rectangles that are wider
  //    (in samples) than they are tall, with a ratio that depends on
  //    the latitude.
  //
  // 2) If we find higher ground in a rectangle, there could still be
  //    a closer higher point in the next concentric rectangle (if the
  //    higher ground we found is outside inscribed ellipse of the
  //    current rectangle).  To guarantee that we search all points as
  //    close as one rectangle in the next concentric rectangle, we
  //    can choose to have concentric rectangles that increase by a
  //    linear factor of sqrt(2) (this guarantees that we check all
  //    points as close as the previous rectangle's four corners in
  //    the next larger rectangle).

  Offsets closestHigherGround(0, 0);
  const float HUGE_DISTANCE = 1 << 30;
  float minDistance = HUGE_DISTANCE;

  int seedx = seedPoint.x();
  int seedy = seedPoint.y();

  int innerleftx = seedx;
  int innerrightx = seedx;
  int innertopy = seedy;
  int innerbottomy = seedy;

  // Initial half height of rectangle
  int dy = 20;
  // Compensate for latitude squish
  int dx = static_cast<int>(ceilf(dy / lngDistanceScaleForRow[seedy]));

  int outerleftx = std::max(0, seedx - dx);
  int outerrightx = std::min(tile->width(), seedx + dx);
  int outertopy = std::max(0, seedy - dy);
  int outerbottomy = std::min(tile->height(), seedy + dy);

  float successive_rectangle_ratio = sqrtf(2);

  VLOG(2) << "Searching for peak at " << seedx << " " << seedy << " "
          << "with elevation " << seedElevation;
  bool foundHigherGroundLastTime = false;
  // Set to true when we need to compute exact distances to the peak
  bool exactDistanceCheck = false;  
  // Done when outer ring is empty
  while ((innerleftx != outerleftx) || (innerrightx != outerrightx) ||
         (innertopy != outertopy) || (innerbottomy != outerbottomy)) {

    VLOG(3) << "Trying outer ring " << outerleftx << " " << outerrightx << " "
            << outertopy << " " << outerbottomy;
    
    // Check all samples between inner ring and outer ring.
    // TODO: This currently checks inside the inner ring too; could be sped up
    for (int y = outertopy; y < outerbottomy; ++y) {
      if (exactDistanceCheck) {
        // Slow path
        assert(peakLocation != nullptr);
        for (int x = outerleftx; x < outerrightx; ++x) {
          if (tile->get(x, y) > seedElevation) {
            float distance = peakLocation->distance(
              tileCoordinateSystem->getLatLng(Offsets(x, y)));
            if (distance < minDistance) {
              VLOG(4) << "Found closer point on slow path: " << x << " " << y;
              minDistance = distance;
              closestHigherGround = Offsets(x, y);
              record.foundHigherGround = true;
            }
          }
        }
      } else {
        // Fast path: approximate distance with average scale factor
        // based on the average latitude of the segment connecting the
        // sample and the seed.
        int averageY = (y + seedy) / 2;
        float lngScaleFactor = lngDistanceScaleForRow[averageY];
        float yDistanceComponent = static_cast<float>((y - seedy) * (y - seedy));
        
        for (int x = outerleftx; x < outerrightx; ++x) {
          if (tile->get(x, y) > seedElevation) {
            float deltaX = (x - seedx) * lngScaleFactor;
            float distance = deltaX * deltaX + yDistanceComponent;
            if (distance < minDistance) {
              VLOG(4) << "Found closer point on fast path: " << x << " " << y << " elev " << tile->get(x, y);
              minDistance = distance;
              closestHigherGround = Offsets(x, y);
              record.foundHigherGround = true;
            }
          }
        }
      }
    }

    // We were checking one extra ring to make sure there wasn't
    // higher ground closer by, and now we've done that.
    if (foundHigherGroundLastTime) {
      break;
    }

    if (record.foundHigherGround && peakLocation != nullptr) {
      // In this case, we've found a higher point close to the seed,
      // but it may not be closest to the peak, which is outside the tile.
      // We need to find a ring size that guarantees that we search all
      // land at least as close to the peak.
      exactDistanceCheck = true;
      LatLng higherGroundLocation = tileCoordinateSystem->getLatLng(closestHigherGround);
      float distancePeakToHigherGround = peakLocation->distance(higherGroundLocation);
      minDistance = distancePeakToHigherGround;

      // A very coarse estimate of the ring size is the peak/higher ground distance.
      // This is crude, but useful in the common case where the peak is just over
      // the tile's border into the neighbor.
      // Degree of latitude is about 111km
      int newdy = (int) (ceil(distancePeakToHigherGround / 111000 * tile->height()));
      
      successive_rectangle_ratio = ((float) newdy) / dy;
      VLOG(3) << "Slow check of neighbor; new dy is " << newdy
              << " with ratio " << successive_rectangle_ratio;
    } else {
      // If we found higher ground and it's inside the inscribed ellipse,
      // we're done: we've checked all samples closer than the higher ground.
      if (minDistance < dy * dy) {
        break;
      }
    }
    
    foundHigherGroundLastTime = record.foundHigherGround;
    
    // increase inner ring only if no higher ground was found
    // Otherwise possible that peak search is not conducted
    if (!record.foundHigherGround) {
      // Old outer ring is new inner ring
      innerleftx = outerleftx;
      innerrightx = outerrightx;
      innertopy = outertopy;
      innerbottomy = outerbottomy;
    }

    // Expand outer ring
    dy = static_cast<int>(ceilf(dy * successive_rectangle_ratio));
    dx = static_cast<int>(ceilf(dx * successive_rectangle_ratio));
    outerleftx = std::max(0, seedx - dx);
    outerrightx = std::min(tile->width(), seedx + dx);
    outertopy = std::max(0, seedy - dy);
    outerbottomy = std::min(tile->height(), seedy + dy);
  }
      
  if (record.foundHigherGround) {
    record.closestHigherGround = tileCoordinateSystem->getLatLng(closestHigherGround);

    LatLng seedLocation(tileCoordinateSystem->getLatLng(seedPoint));
    record.distance = seedLocation.distance(record.closestHigherGround);
  }

  delete [] lngDistanceScaleForRow;
  return record;
}

IsolationRecord IsolationFinder::checkNeighboringTile(float lat, float lng, const LatLng *peakLocation,
                                                      Offsets seedCoords, Elevation elev) const {
  VLOG(2) << "Possibly considering neighbor tile " << lat << " " << lng;
  
  // Don't even bother loading tile if we know if's all lower ground
  Elevation maxElevation;
  if (mCache->getMaxElevation(lat, lng, &maxElevation) && elev > maxElevation) {
    return IsolationRecord();
  }
  // Look in neighbor for nearest higher ground to close point
  CoordinateSystem *tileCoordinateSystem = mFormat.coordinateSystemForOrigin(lat, lng);

  Tile *neighbor = mCache->getOrLoad(lat, lng, *tileCoordinateSystem);
  if (neighbor != nullptr) {
    // TODO: This asssumes that neighbor tile has same size as this tile.  Could use lat/lng instead.
    return findIsolation(neighbor, tileCoordinateSystem, peakLocation, seedCoords, elev);
  }
  return IsolationRecord();  // Nothing found
}
