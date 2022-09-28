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
 * A rectangular grid of terrain samples, without a geographic reference.
 */

#ifndef _TILE_H_
#define _TILE_H_

#include "primitives.h"

class Tile {
public:

  // Tile takes ownership of samples, which will be deallocated later via free().
  Tile(int width, int height, Elevation *samples);
  ~Tile();
  
  int width() const { return mWidth; }
  int height() const { return mHeight; }

  bool isInExtents(int x, int y) const {
    return (x >= 0) && (x < mWidth) && (y >= 0) && (y < mHeight);
  }
  bool isInExtents(Offsets offsets) const {
    return isInExtents(offsets.x(), offsets.y());
  }
  
  Elevation get(Offsets offsets) const {
    return get(offsets.x(), offsets.y());
  }
  
  Elevation get(int x, int y) const {
    return mSamples[y * mWidth + x];
  }

  void set(int x, int y, Elevation elevation) {
    mSamples[y * mWidth + x] = elevation;
  }
  
  Elevation maxElevation() const {
    return mMaxElevation;
  }

  // Flip elevations so that depressions and mountains are swapped.
  // No-data values are left unchanged.
  void flipElevations();

  // Missing data in source
  static constexpr Elevation NODATA_ELEVATION = -32768;
  
private:
  Tile();
  
  int mWidth;
  int mHeight;

  Elevation mMaxElevation;
  
  Elevation *mSamples;

  // After setting some values, need to recompute max, which is cached
  void recomputeMaxElevation();

  Elevation computeMaxElevation() const;
};


#endif  // _TILE_H_
