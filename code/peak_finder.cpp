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


#include "peak_finder.h"

#include <memory.h>
#include <stdlib.h>
#include <stack>

using std::vector;
using std::stack;

PeakFinder::PeakFinder(const Tile *tile) {
  mTile = tile;
}

vector<Offsets> PeakFinder::findPeaks() const {
  vector<Offsets> peaks;

  // Middle of tile excluding edges: every sample has all 8-neighbors
  for (int y = 1; y < mTile->height() - 1; ++y) {
    for (int x = 1; x < mTile->width() - 1; ++x) {
      Elevation elev = mTile->get(x, y);
      // At least as high as all 8-neighbors?
      if (elev != Tile::NODATA_ELEVATION
          && elev >= mTile->get(x - 1, y) && elev >= mTile->get(x + 1, y)
          && elev >= mTile->get(x, y - 1) && elev >= mTile->get(x, y + 1)
          && elev >= mTile->get(x - 1, y - 1) && elev >= mTile->get(x - 1, y + 1)
          && elev >= mTile->get(x + 1, y - 1) && elev >= mTile->get(x + 1, y + 1)) {
        peaks.push_back(Offsets(x, y));
      }
    }
  }

  // Each tile contains one row and column of overlap with each of its 4-neighbors.
  // Thus, it's sufficient to check only the top and left edges for peaks.  The neighbors
  // will find any peaks on the right and bottom edges.
  //
  // It would be expensive to load neighboring tiles to look for peaks.  Since it's OK
  // to find extra peaks (but not OK to miss any), we simply restrict our checks on the
  // top and left edges to those samples that are in this tile.  Any bogus peaks that are
  // found this way will be filtered out later since their isolation will be tiny.

  // Top edge
  for (int x = 1; x < mTile->width() - 1; ++x) {
    Elevation elev = mTile->get(x, 0);
    
    // 5 neighbors in tile
    if (elev != Tile::NODATA_ELEVATION
        && elev >= mTile->get(x - 1, 0) && elev >= mTile->get(x + 1, 0)
        && elev >= mTile->get(x, 1) && elev >= mTile->get(x - 1, 1)
        && elev >= mTile->get(x + 1, 1)) {
      peaks.push_back(Offsets(x, 0));
    }
  }
  
  // Left edge
  for (int y = 1; y < mTile->height() - 1; ++y) {
    Elevation elev = mTile->get(0, y);

    // 5 neighbors in tile
    if (elev != Tile::NODATA_ELEVATION
        && elev >= mTile->get(1, y)
        && elev >= mTile->get(0, y - 1) && elev >= mTile->get(0, y + 1)
        && elev >= mTile->get(1, y - 1) && elev >= mTile->get(1, y + 1)) {
      peaks.push_back(Offsets(0, y));
    }
  }

  // Upper-left pixel
  Elevation elev = mTile->get(0, 0);
  // 3 neighbors in tile
  if (elev != Tile::NODATA_ELEVATION
      && elev >= mTile->get(1, 0)
      && elev >= mTile->get(0, 1)
      && elev >= mTile->get(1, 1)) {
    peaks.push_back(Offsets(0, 0));
  }

  peaks = filter(mTile, peaks);
  
  return peaks;
}

vector<Offsets> PeakFinder::filter(const Tile *tile, const vector<Offsets> &peaks) const {
  // In flat areas, we have lots of peaks right next to each other, because we
  // had to use >= in determining peaks to correctly find a peak on a flat plateau.
  // But we only need one representative peak in the flat area.
  //
  // Since peaks are by definition as high as all their neighbors, no plateau touches
  // another one.  We set up a new tile, initially all 0, and write 1
  // to the corresponding cell for each peak.  Then we walk the tile, and when we encounter a non-zero
  // cell (i.e. a potential peak), we flood fill from there, blowing away (setting to 0)
  // any touching peak, which will by definition have the same elevation.
  //
  // This is an optimization to reduce the number of candidate peaks.

  int width = tile->width();
  int height = tile->height();
  int num_bytes = sizeof(Elevation) * width * height;
  Elevation *elevations = (Elevation *) malloc(num_bytes);
  memset(elevations, 0, num_bytes);
  for (auto peak : peaks) {
    elevations[peak.x() + width * peak.y()] = 1;
  } 
  
  // TODO: This iteration tends to get the NW corner of each flat area.  It would be nicer
  // to get something at the center.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (elevations[x + width * y] != 0) {
        // Flood fill with 0s from here
        stack<Offsets> pending;
        pending.push(Offsets(x, y));
        while (!pending.empty()) {
          Offsets point = pending.top();
          pending.pop();
          if (elevations[point.x() + width * point.y()] == 0) {
            continue;
          }
          elevations[point.x() + width * point.y()] = 0;

          if (point.x() > 0) {
            pending.push(Offsets(point.x() - 1, point.y()));
          }
          if (point.x() < width - 1) {
            pending.push(Offsets(point.x() + 1, point.y()));
          }
          if (point.y() > 0) {
            pending.push(Offsets(point.x(), point.y() - 1));
          }
          if (point.y() < height - 1) {
            pending.push(Offsets(point.x(), point.y() + 1));
          }
        }

        // Put back the original peak (it was overwritten with 0)
        elevations[x + width * y] = 1;
      }
    }
  }

  // Build up new peaks
  vector<Offsets> newPeaks;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (elevations[x + width * y] != 0) {
        newPeaks.push_back(Offsets(x, y));
      }
    }
  }
    
  free(elevations);

  return newPeaks;
}
