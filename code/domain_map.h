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

/*
 * A rectangular raster, of the same size as an associated elevation
 * tile.  Each element of the raster describes whether the corresponding
 * flat area in the tile is a peak or saddle.  Peaks have positive
 * values, and saddles have negative values.
 *
 * The special value "GenericFlatArea" indicates that the pixel is
 * part of a multi-pixel flat area that is neither a peak or a saddle.
 */

#ifndef _DOMAIN_MAP_H_
#define _DOMAIN_MAP_H_

#include "tile.h"
#include "pixel_array.h"

#include <stack>
#include <vector>

class DomainMap {
public:
  explicit DomainMap(const Tile *tile);

  typedef int32 Pixel;
  static const Pixel EmptyPixel = PixelArray<Pixel>::EmptyPixel;

  // This value indicates that the pixel is part of a flat area that
  // is neither a peak nor a saddle.
  static const Pixel GenericFlatArea = -999999;
  
  struct Boundary {
    std::vector<Offsets::Value> higherPoints;
  };
  
  // Find the flat region containing the given point, then fills in
  // boundary with the points on the boundary higher than the given
  // point.  A given point may appear in the boundary multiple times.
  void findFlatArea(int x, int y, Boundary *boundary);

  // Fill the 8-connected flat region at (x, y) with the given value.
  void fillFlatArea(int x, int y, Pixel value);
  
  Pixel get(Offsets offsets) const {
    return get(offsets.x(), offsets.y());
  }

  Pixel get(int x, int y) const {
    return mPixels.get(x, y);
  }

  // Find the (approximately) closest point to the given location that
  // has the given pixel value.
  Offsets findClosePointWithValue(Offsets location, Pixel value) const;

private:
  const Tile *mTile;
  PixelArray<Pixel> mPixels;

  // Used internally to detect whether a given pixel has already been touched
  // during a given operation.
  PixelArray<Pixel> mMarkers;

  // An unused marker value for use during the next operation
  int mMarkerValue;  

  // A Range is the set of pixels at [xmin,y] through [xmax,y] inclusive.
  // All pixels in a Range are known to be at the target elevation and are marked.
  struct Range {
    Range(Coord minx, Coord maxx, Coord ycoord) {
      xmin = minx;
      xmax = maxx;
      y = ycoord;
    }
    
    Coord xmin, xmax;
    Coord y;
  };

  std::stack<Range> mPendingRanges;
};

#endif  // _DOMAIN_MAP_H_
