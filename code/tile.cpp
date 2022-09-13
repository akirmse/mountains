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

#include "tile.h"
#include "math_util.h"
#include "util.h"

#include "easylogging++.h"

#include <algorithm>
#include <assert.h>
#include <stdlib.h>

Tile::Tile(int width, int height, Elevation *samples) {
  mWidth = width;
  mHeight = height;
  mSamples = samples;
  
  mMaxElevation = 0;

  recomputeMaxElevation();
}

Tile::~Tile() {
  free(mSamples);
}

void Tile::flipElevations() {
  for (int i = 0; i < mWidth * mHeight; ++i) {
    Elevation elev = mSamples[i];
    if (elev != NODATA_ELEVATION) {
      mSamples[i] = -elev;
    }
  }
}

void Tile::recomputeMaxElevation() {
  mMaxElevation = computeMaxElevation();
}

Elevation Tile::computeMaxElevation() const {
  Elevation max_elevation = 0;
  for (int i = 0; i < mWidth * mHeight; ++i) {
    Elevation elev = mSamples[i];
    max_elevation = std::max(max_elevation, elev);
  }

  return max_elevation;
}
