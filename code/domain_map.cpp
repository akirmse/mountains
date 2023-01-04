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

#include "domain_map.h"
#include "easylogging++.h"

#include <math.h>

using std::stack;
using std::vector;

DomainMap::DomainMap(const Tile *tile) :
    mPixels(tile->width(), tile->height()),
    mMarkers(tile->width(), tile->height()) {
  mTile = tile;
  mMarkerValue = 1;
 }

void DomainMap::findFlatArea(int x, int y, Boundary *boundary) {
  // Use a new marker value so we don't see the results of any previous operations
  mMarkerValue += 1;
  boundary->higherPoints.clear();

  Elevation elev = mTile->get(x, y);

  // Flood fill
  mPendingRanges.push(Range(x, x, y));
  
  while (!mPendingRanges.empty()) {
    Range range(mPendingRanges.top());
    mPendingRanges.pop();

    // Extend range to the left
    while (true) {
      Coord leftx = range.xmin - 1;
      if (leftx < 0) {
        break;
      }

      Elevation neighbor_elev = mTile->get(leftx, range.y);
      if (neighbor_elev != elev) {
        // Higher neighbor?
        if (neighbor_elev != Tile::NODATA_ELEVATION && neighbor_elev > elev) {
          boundary->higherPoints.push_back(Offsets(leftx, range.y).value());
        }
        break;
      }
      range.xmin = leftx;
    }

    // Extend range to the right
    while (true) {
      Coord rightx = range.xmax + 1;
      if (rightx >= mTile->width()) {
        break;
      }

      Elevation neighbor_elev = mTile->get(rightx, range.y);
      if (neighbor_elev != elev) {
        // Higher neighbor?
        if (neighbor_elev != Tile::NODATA_ELEVATION && neighbor_elev > elev) {
          boundary->higherPoints.push_back(Offsets(rightx, range.y).value());
        }
        break;
      }
      range.xmax = rightx;
    }

    // Mark range as visited
    mMarkers.setRange(range.xmin, range.y, mMarkerValue, range.xmax - range.xmin + 1);

    // Find adjacent ranges above
    if (range.y > 0) {
      Coord lo = -1;
      Coord topy = range.y - 1;
      Coord maxx = std::min(range.xmax + 1, mTile->width() - 1);
      for (Coord topx = range.xmin - 1; topx <= maxx; ++topx) {
        if (mTile->isInExtents(topx, topy)) {
          Elevation neighbor_elev = mTile->get(topx, topy);
          if (neighbor_elev == elev) {
            if (lo == -1) {
              lo = topx;  // start of a range
            }
          } else {
            if (neighbor_elev != Tile::NODATA_ELEVATION && neighbor_elev > elev) {
              boundary->higherPoints.push_back(Offsets(topx, topy).value());
            }
            if (lo != -1) {
              // End of a range.  Add it if it hasn't been filled before.
              // It's enough to check the leftmost pixel, since the whole
              // range is either empty or non-empty.
              if (mMarkers.get(lo, topy) != mMarkerValue) {
                mPendingRanges.push(Range(lo, topx - 1, topy));
              }
              lo = -1;
            }
          }
        }
      }
      // Didn't encounter end of range
      if (lo != -1 && mMarkers.get(lo, topy) != mMarkerValue) {
        mPendingRanges.push(Range(lo, maxx, topy));
      }
    }

    // Find adjacent ranges below
    if (range.y < mTile->height() - 1) {
      Coord lo = -1;
      Coord bottomy = range.y + 1;
      Coord maxx = std::min(range.xmax + 1, mTile->width() - 1);
      for (Coord bottomx = range.xmin - 1; bottomx <= maxx; ++bottomx) {
        if (mTile->isInExtents(bottomx, bottomy)) {
          Elevation neighbor_elev = mTile->get(bottomx, bottomy);
          if (neighbor_elev == elev) {
            if (lo == -1) {
              lo = bottomx;  // start of a range
            }
          } else {
            if (neighbor_elev != Tile::NODATA_ELEVATION && neighbor_elev > elev) {
              boundary->higherPoints.push_back(Offsets(bottomx, bottomy).value());
            }
            if (lo != -1) {
              // End of a range.  Add it if it hasn't been filled before.
              // It's enough to check the leftmost pixel, since the whole
              // range is either empty or non-empty.
              if (mMarkers.get(lo, bottomy) != mMarkerValue) {
                mPendingRanges.push(Range(lo, bottomx - 1, bottomy));
              }
              lo = -1;
            }
          }
        }
      }
      // Didn't encounter end of range
      if (lo != -1 && mMarkers.get(lo, bottomy) != mMarkerValue) {
        mPendingRanges.push(Range(lo, maxx, bottomy));
      }
    }
  }
}

void DomainMap::fillFlatArea(int x, int y, Pixel value) {
  // Flood fill based on horizontal ranges
  Elevation elev = mTile->get(x, y);

  mPendingRanges.push(Range(x, x, y));
  
  while (!mPendingRanges.empty()) {
    Range range(mPendingRanges.top());
    mPendingRanges.pop();

    // Extend range to the left
    while (true) {
      Coord leftx = range.xmin - 1;
      if (leftx < 0 || mTile->get(leftx, range.y) != elev) {
        break;
      }
      range.xmin = leftx;
    }

    // Extend range to the right
    while (true) {
      Coord rightx = range.xmax + 1;
      if (rightx >= mTile->width() || mTile->get(rightx, range.y) != elev) {
        break;
      }
      range.xmax = rightx;
    }

    // Fill range
    mPixels.setRange(range.xmin, range.y, value, range.xmax - range.xmin + 1);

    // Find adjacent ranges above
    if (range.y > 0) {
      Coord lo = -1;
      Coord topy = range.y - 1;
      for (Coord topx = range.xmin - 1; topx <= range.xmax + 1; ++topx) {
        if (!mTile->isInExtents(topx, topy) || mTile->get(topx, topy) != elev) {
          if (lo != -1) {
            // End of a range.  Add it if it hasn't been filled before.
            // It's enough to check the leftmost pixel, since the whole
            // range is either empty or non-empty.
            if (mPixels.get(lo, topy) == EmptyPixel) {
              mPendingRanges.push(Range(lo, topx - 1, topy));
            }
            lo = -1;
          }
        } else {
          if (lo == -1) {
            lo = topx;  // start of a range
          }
        }
      }
      // Didn't encounter end of range
      if (lo != -1 && mPixels.get(lo, topy) == EmptyPixel) {
        mPendingRanges.push(Range(lo, range.xmax + 1, topy));
      }
    }

    // Find adjacent ranges below
    if (range.y < mTile->height() - 1) {
      Coord lo = -1;
      Coord bottomy = range.y + 1;
      for (Coord bottomx = range.xmin - 1; bottomx <= range.xmax + 1; ++bottomx) {
        if (!mTile->isInExtents(bottomx, bottomy) || mTile->get(bottomx, bottomy) != elev) {
          if (lo != -1) {
            // End of a range
            if (mPixels.get(lo, bottomy) == EmptyPixel) {
              mPendingRanges.push(Range(lo, bottomx - 1, bottomy));
            }
            lo = -1;
          }
        } else {
          if (lo == -1) {
            lo = bottomx;  // start of a range
          }
        }
      }
      // Didn't encounter end of range
      if (lo != -1 && mPixels.get(lo, bottomy) == EmptyPixel) {
        mPendingRanges.push(Range(lo, range.xmax + 1, bottomy));
      }
    }
  }
}

Offsets DomainMap::findClosePointWithValue(Offsets location, Pixel value) const {
  if (mPixels.get(location.x(), location.y()) == value) {
    return location;
  }

  // This isn't particularly efficient, but for the kind of local searches we're doing
  // it should be good enough.
  for (int radius = 1; radius < mTile->width(); ++radius) {
    int x = location.x() - radius;
    int y = location.y() - radius;
    int dx = 1;
    int dy = 0;
    while (true) {
      if (mTile->isInExtents(x, y) && mPixels.get(x, y) == value) {
        return Offsets(x, y);
      }

      if (x == location.x() + radius && y == location.y() - radius) {
        // Top right corner: go down
        dx = 0;
        dy = 1;
      } else if (x == location.x() + radius && y == location.y() + radius) {
        // Bottom right corner: go left
        dx = -1;
        dy = 0;
      } else if (x == location.x() - radius && y == location.y() + radius) {
        // Bottom left corner: go up
        dx = 0;
        dy = -1;
      }

      x += dx;
      y += dy;

      if (x == location.x() - radius && y == location.y() - radius) {
        // Return to top left corner: done
        break;
      }
    }
  }

  LOG(ERROR) << "Failed to find point near " << location.x() << " " << location.y()
             << " with value " << value;
  return location;
}
