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

#include "tree_builder.h"
#include "coordinate_system.h"
#include "divide_tree.h"
#include "easylogging++.h"

#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include <stack>
#include <string>

using std::stack;
using std::string;
using std::vector;

// This class is responsible for converting a terrain tile into a
// divide tree.  The algorithm is:
//
// * Find all peaks and saddles.  A peak is a flat area higher than
// its entire boundary.  A saddle is a flat area with at least two
// non-touching higher areas in its boundary.  If there are more than
// two higher areas, the saddle must be split into multiple saddles,
// each with a multiplicity of 2.
//
// * Walk up divides (via steepest ascent) from each saddle to two
// peaks.  If the walk encounters a flat region, find the highest
// point adjacent to the flat region.
//
// * If the two walks reach the same peak, the saddle is false and
// is discarded.  Otherwise, an edge is added to the divide tree
// between the two peaks.  If this would create a cycle, the edge with
// the lowest saddle on the cycle is removed.  This lowest saddle
// is called a "basin saddle" and is not used past this point.
//
// * Compute runoffs around the edge of the tile, and add runoff->peak
// edges to the divide tree by walking uphill from each runoff.

TreeBuilder::TreeBuilder(const Tile *tile, const CoordinateSystem &coordinateSystem) :
    mDomainMap(tile) {
  mTile = tile;
  mCoordinateSystem = std::unique_ptr<CoordinateSystem>(coordinateSystem.clone());
}

DivideTree *TreeBuilder::buildDivideTree() {
  VLOG(1) << "Finding peaks and saddles";
  findExtrema();

  VLOG(1) << "Building divide tree";
  return generateDivideTree();
}

void TreeBuilder::findExtrema() {
  // Avoid VLOG locking in certain performance-critical loops
  bool vlog2 = VLOG_IS_ON(2);
  bool vlog4 = VLOG_IS_ON(4);
  
  DomainMap::Boundary boundary;
  vector<Offsets> segmentHighPoints;
  for (int y = 0; y < mTile->height(); ++y) {
    for (int x = 0; x < mTile->width(); ++x) {
      Elevation elev = mTile->get(x, y);
      
      // Skip nodata
      if (elev == Tile::NODATA_ELEVATION) {
        continue;
      }

      // Skip points already marked from another flat area
      if (mDomainMap.get(x, y) != DomainMap::EmptyPixel) {
        continue;
      }

      mDomainMap.findFlatArea(x, y, &boundary);
      
      // If no higher boundary points, this is a peak
      if (boundary.higherPoints.empty()) {
        int peakId = static_cast<int>(mPeaks.size() + 1);
        mDomainMap.fillFlatArea(x, y, peakId);
        
        // TODO: Pick a point near the middle of the flat region
        mPeaks.push_back(Peak(Offsets(x, y), elev));
        if (vlog2) {  // Performance optimization
          LatLng pos = mCoordinateSystem->getLatLng(Offsets(x, y));
          VLOG(2) << "Peak #" << peakId << " at " << x << " " << y
                  << " at " << pos.latitude() << " " << pos.longitude();
        }

        continue;
      }

      // Look for saddles
      // Quick reject: can't be a saddle if there's only 1 higher point
      if (boundary.higherPoints.size() < 2) {
        mDomainMap.fillFlatArea(x, y, DomainMap::GenericFlatArea);
        continue;
      }

      // Compute connected segments of higher points in boundary
      segmentHighPoints.clear();
      int segmentWithHighestPoint = 0;

      // Keeping the vector sorted turns out to be a speed win
      std::sort(boundary.higherPoints.begin(), boundary.higherPoints.end());

      // Remove duplicates -- makes a difference on enormous flat areas
      int size = (int) boundary.higherPoints.size();
      if (size > 100) {
        int to = 0;
        Offsets::Value last = -1;
        for (int i = 0; i < size; ++i) {
          Offsets::Value next = boundary.higherPoints[i];
          if (last != next) {
            boundary.higherPoints[to] = next;
            last = next;
            to += 1;
          }
        }
        boundary.higherPoints.resize(to);
      }      
      
      while (!boundary.higherPoints.empty()) {
        // Store high point in segment
        Offsets higherPoint(*(boundary.higherPoints.begin()));

        // "Flood-fill" boundary, finding any higher points connected to this one
        mPendingStack.push(higherPoint);
        Offsets highestPointInSegment = higherPoint;
        Elevation maxHeightInSegment = mTile->get(higherPoint);

        while (!mPendingStack.empty()) {
          Offsets point = mPendingStack.top();
          mPendingStack.pop();
          // Fast erase from sorted vector, see:
          // http://stackoverflow.com/questions/26719144/how-to-erase-a-value-efficiently-from-a-sorted-vector
          auto pr = std::equal_range(boundary.higherPoints.begin(),
                                     boundary.higherPoints.end(),
                                     point.value());
          boundary.higherPoints.erase(pr.first, pr.second);
          
          if (mTile->get(point) > maxHeightInSegment) {
            highestPointInSegment = point;
            maxHeightInSegment = mTile->get(point);
          }
          
          // Add any neighbors in higher points set
          for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
              Offsets neighbor(point.x() + dx, point.y() + dy);
              if (std::binary_search(boundary.higherPoints.begin(),
                                     boundary.higherPoints.end(),
                                     neighbor.value())) {
                mPendingStack.push(neighbor);
              }
            }
          }
        }

        segmentHighPoints.push_back(highestPointInSegment);
        if (maxHeightInSegment > mTile->get(segmentHighPoints[segmentWithHighestPoint])) {
          segmentWithHighestPoint = static_cast<int>(segmentHighPoints.size() - 1);
        }
      }

      // If there are N (>= 2) higher segments bordering this flat
      // area, then there are N-1 saddles in the flat area.  We need
      // to generate these saddles in a way that the walks across the
      // flat area don't cross.  (This isn't strictly necessary for
      // prominence, but it would be needed if we generate domain
      // maps.)  To do this, we find the highest segment, then generate
      // a saddle between this segment and the highest point of
      // each of the other segments.
      int numSegments = static_cast<int>(segmentHighPoints.size());
      if (numSegments < 2) {
        // Flat area, but not a saddle
        if (vlog4) {  // Performance optimization
          VLOG(4) << "Rejecting flat area " << x << " " << y
                  << " elev " << elev << " multiplicity " << numSegments;
        }
        mDomainMap.fillFlatArea(x, y, DomainMap::GenericFlatArea);
        continue;
      }

      int filledSaddleId = 0;
      for (int i = 0; i < numSegments; ++i) {
        if (i != segmentWithHighestPoint) {
          // Saddles have negative IDs
          int saddleId = -static_cast<int>(mSaddles.size() + 1);
          VLOG(2) << "Saddle " << saddleId << " at " << x << " " << y
                  << " elev " << elev << " multiplicity " << numSegments;

          // Mark flat area; only need to do this once, and which saddle ID to use is arbitrary
          if (filledSaddleId == 0) {
            mDomainMap.fillFlatArea(x, y, saddleId);
            filledSaddleId = saddleId;
          }

          PerSaddleInfo info(segmentHighPoints[i], segmentHighPoints[segmentWithHighestPoint]);

          // In an absolutely enormous saddle with tons of segments, it's too slow
          // to find nice-looking saddle points.  (It's likely that the saddle is near sea level
          // and has no prominence anyway.)
          Offsets closePoint(x, y);
          if (numSegments < 500) {
            // Try to find a location for the saddle that will look nice.  Put it
            // in the flat area as close as possible to the midpoint between the boundary
            // high points.
            Offsets midpoint((info.rise1.x() + info.rise2.x()) / 2,
                             (info.rise1.y() + info.rise2.y()) / 2);
            closePoint = mDomainMap.findClosePointWithValue(midpoint, filledSaddleId);
          }
          
          Saddle saddle(closePoint, elev);
          mSaddles.push_back(saddle);
          mSaddleInfo.push_back(info);
        }
      }
    }
  }

  findRunoffs();
}

void TreeBuilder::findRunoffs() {
  // Walk tile border, going left to right, and top to bottom.  It's important that
  // the directions match in neighboring tiles so that runoffs are found in exactly
  // the same places in overlapping rows and columns.  That would not happen if
  // we e.g. walked the border clockwise.
  //
  // While walking, look for a fall after a rise.
  int x = 0;
  int y = 0;
  int dx = 1;
  int dy = 0;
  bool risingOrFlat = false;
  Elevation elev = mTile->get(x, y);
  Elevation lastElevation = elev;
  if (elev != Tile::NODATA_ELEVATION) {
    mRunoffs.push_back(Runoff(Offsets(0, 0), elev, 1));
  }
  
  while (true) {
    elev = mTile->get(x, y);

    if (elev != Tile::NODATA_ELEVATION &&
        (lastElevation == Tile::NODATA_ELEVATION || elev > lastElevation)) {
      // Start or continue of a rise
      risingOrFlat = true;
    } else if (risingOrFlat &&
               (elev == Tile::NODATA_ELEVATION || elev < lastElevation)) {
      // Fell after a rise: previous point was runoff.
      // 2 neighboring quadrants, because this is along an edge and not at corner.
      mRunoffs.push_back(Runoff(Offsets(x - dx, y - dy), lastElevation, 2));
      risingOrFlat = false;
    }
    lastElevation = elev;
    
    // Always generate runoffs at corners, because there may be a peak
    // or saddle there that involves pixels we haven't even seen yet.
    if (x == mTile->width() - 1 && y == 0) {  // upper right
      if (elev != Tile::NODATA_ELEVATION) {
        mRunoffs.push_back(Runoff(Offsets(x, y), elev, 1));
        risingOrFlat = false;
      }
      dx = 0;
      dy = 1;
    } else if (x == mTile->width() - 1 && y == mTile->height() - 1) {  // lower right
      if (dx == 1) {
        // Hit lower right while traveling right.  Done.
        break;
      }
      if (elev != Tile::NODATA_ELEVATION) {
        mRunoffs.push_back(Runoff(Offsets(x, y), elev, 1));
      }
      risingOrFlat = false;
      x = 0;  // Go back to upper left and go down
      y = 0;
      lastElevation = mTile->get(0, 0);
      dx = 0;
      dy = 1;
    } else if (x == 0 && y == mTile->height() - 1) {  // lower left
      if (elev != Tile::NODATA_ELEVATION) {
        mRunoffs.push_back(Runoff(Offsets(x, y), elev, 1));
        risingOrFlat = false;
      }
      dx = 1;
      dy = 0;
    }

    x += dx;
    y += dy;
  }

  // Determine if each runoff is within the flat area of a peak
  for (Runoff &runoff : mRunoffs) {
    runoff.insidePeakArea = (mDomainMap.get(runoff.location) > 0);
  }
}

DivideTree *TreeBuilder::generateDivideTree() {
  DivideTree *tree = new DivideTree(*mCoordinateSystem, mPeaks, mSaddles, mRunoffs);
  
  int saddleIndex = 0;
  for (Saddle &saddle : mSaddles) {
    saddleIndex += 1;
    const PerSaddleInfo &info = mSaddleInfo[saddleIndex - 1];
    vector<Offsets> path1 = walkUpToPeak(info.rise1);
    vector<Offsets> path2 = walkUpToPeak(info.rise2);
    
    if (path1.empty() || path2.empty()) {
      LatLng pos = mCoordinateSystem->getLatLng(saddle.location);
      LOG(ERROR) << "Failed to connect saddle " << saddleIndex << " to peak from "
                 << saddle.location.x() << " " << saddle.location.y() << " "
                 << pos.latitude() << " " << pos.longitude();
      saddle.type = Saddle::Type::ERROR_SADDLE;
      continue;
    }

    int peak1 = mDomainMap.get(path1.back());
    int peak2 = mDomainMap.get(path2.back());
    if (peak1 == peak2) {
      // This is not really a saddle; skip it
      VLOG(4) << "Got false saddle " << saddleIndex << " for peak " << peak1;
      saddle.type = Saddle::Type::FALSE_SADDLE;
      continue;
    }

    saddle.type = Saddle::Type::PROM;
    VLOG(2) << "Got real saddle " << saddleIndex << " for peaks " << peak1 << " " << peak2;
    
    // Add edge to divide tree
    int basinSaddleId = tree->maybeAddEdge(peak1, peak2, saddleIndex);
    if (basinSaddleId != DivideTree::Node::Null) {
      VLOG(3) << "Got basin saddle " << basinSaddleId << " for peaks " << peak1 << " " << peak2;
      mSaddles[basinSaddleId - 1].type = Saddle::Type::BASIN;
    }
  }
  tree->setSaddles(mSaddles);  // We've changed the saddle types

  // Add runoffs to divide tree after finding associated peak by uphill walk
  for (int index = 0; index < (int) mRunoffs.size(); ++index) {
    const Runoff &runoff = mRunoffs[index];
    vector<Offsets> path = walkUpToPeak(runoff.location);
    if (path.empty()) {
      LatLng pos = mCoordinateSystem->getLatLng(runoff.location);
      LOG(ERROR) << "Failed to connect runoff " << saddleIndex << " to peak from "
                 << runoff.location.x() << " " << runoff.location.y() << " "
                 << pos.latitude() << " " << pos.longitude();
      continue;
    }
    int peak = mDomainMap.get(path.back());
    tree->addRunoffEdge(peak, index);
  }

  // Delete false saddles
  tree->compact();

  if (VLOG_IS_ON(2)) {
    tree->debugPrint();
  }
  
  // Count saddle types, for debugging
  auto basin_saddle_count = std::count_if(mSaddles.begin(), mSaddles.end(),
                                          [](const Saddle &saddle) {
                                            return saddle.type == Saddle::Type::BASIN;
                                          });
  auto prom_saddle_count = std::count_if(mSaddles.begin(), mSaddles.end(),
                                         [](const Saddle &saddle) {
                                           return saddle.type == Saddle::Type::PROM;
                                         });
    
  VLOG(1) << "Found " << mPeaks.size() << " peaks, " << prom_saddle_count << " prom saddles, "
          << basin_saddle_count << " basin saddles, " << mRunoffs.size() << " runoffs";

  return tree;
}

vector<Offsets> TreeBuilder::walkUpToPeak(Offsets startPoint) {
  vector<Offsets> path;
  
  Offsets point = startPoint;
  DomainMap::Pixel domainPixel = 0;
  while (true) {
    path.push_back(point);
    domainPixel = mDomainMap.get(point.x(), point.y());
    
    // Found a peak?
    if (domainPixel > 0) {
      break;
    }
    
    // Found a saddle?
    if (domainPixel < 0 && domainPixel != DomainMap::GenericFlatArea &&
        domainPixel != DomainMap::EmptyPixel) {
      VLOG(2) << "During walkup, encountered saddle " << domainPixel;
      // Exit saddle up steepest boundary point
      point = getSaddleInfo(domainPixel).rise1;
      continue;
    }
    
    // Ascend via steepest neighbor
    Offsets newPoint = findSteepestNeighbor(point);
    if (point == newPoint) {
      // No higher neighbor; need to check boundary of entire flat area
      DomainMap::Boundary boundary;
      mDomainMap.findFlatArea(point.x(), point.y(), &boundary);

      Elevation highestElevation = mTile->get(point);
      for (auto value : boundary.higherPoints) {
        Offsets neighbor(value);
        if (mTile->get(neighbor) > highestElevation) {
          highestElevation = mTile->get(neighbor);
          newPoint = neighbor;
        }
      }

      // Still no higher point?  Bug.
      if (point == newPoint) {
        LatLng pos = mCoordinateSystem->getLatLng(point);
        LOG(ERROR) << "Couldn't find higher neighbor for " << point.x() << " " << point.y()
                   << " elev " << mTile->get(point) << " at " << pos.latitude() << " " << pos.longitude();
        LOG(ERROR) << "Path length was " << path.size();
        // Indicate badness to caller
        return vector<Offsets>();
      }
    }
    
    point = newPoint;
  }

  if (domainPixel != DomainMap::EmptyPixel) {
    VLOG(2) << "Found path from saddle to peak " << domainPixel << " of length " << path.size();
  }
  
  return path;
}

const Saddle &TreeBuilder::getSaddle(DomainMap::Pixel domainPixel) const {
  // saddle values are negative and start at -1
  return mSaddles[-domainPixel - 1];
}

const TreeBuilder::PerSaddleInfo &TreeBuilder::getSaddleInfo(DomainMap::Pixel domainPixel) const {
  return mSaddleInfo[-domainPixel - 1];
}

Offsets TreeBuilder::findSteepestNeighbor(Offsets point) const {
  Elevation maxElev = -30000;
  Offsets maxPoint = point;
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      Offsets neighbor(point.x() + dx, point.y() + dy);
      if (mTile->isInExtents(neighbor) && mTile->get(neighbor) > maxElev) {
        maxElev = mTile->get(neighbor);
        maxPoint = neighbor;
      }
    }
  }

  return maxPoint;
}
