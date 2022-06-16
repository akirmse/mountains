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

Tile::Tile(int width, int height, Elevation *samples,
           float minLat, float minLng, float maxLat, float maxLng) {
  mWidth = width;
  mHeight = height;
  mSamples = samples;
  mMinLat = minLat;
  mMinLng = minLng;
  mMaxLat = maxLat;
  mMaxLng = maxLng;
  
  mLngDistanceScale = nullptr;
  mMaxElevation = 0;

  precomputeTileAfterLoad();
}

Tile::~Tile() {
  free(mSamples);
  free(mLngDistanceScale);
}

void Tile::setLatLng(float latitude, float longitude, Elevation elevation) {
  Offsets offsets = toOffsets(latitude, longitude);
  int x = offsets.x();
  int y = offsets.y();
  
  assert(x >= 0);
  assert(x < mWidth);
  assert(y >= 0);
  assert(y < mHeight);

  mSamples[y * mWidth + x] = elevation;
}

bool Tile::isInExtents(float latitude, float longitude) const {
  Offsets offsets = toOffsets(latitude, longitude);
  return isInExtents(offsets.x(), offsets.y());
}

int Tile::numVerticalSamplesForDistance(float distance) const {
  // Degree of latitude is about 111km
  return (int) (ceil(distance / 111000 * mHeight));
}

void Tile::flipElevations() {
  for (int i = 0; i < mWidth * mHeight; ++i) {
    Elevation elev = mSamples[i];
    if (elev != NODATA_ELEVATION) {
      mSamples[i] = -elev;
    }
  }
}

void Tile::precomputeTileAfterLoad() {
  // Precompute max elevation
  recomputeMaxElevation();
  
  // Precompute distance scale factors
  mLngDistanceScale = (float *) malloc(sizeof(float) * mHeight);
  for (int y = 0; y < mHeight; ++y) {
    LatLng point(latlng(Offsets(0, y)));
    mLngDistanceScale[y] = cosf(degToRad(point.latitude()));
  }
}

void Tile::recomputeMaxElevation() {
  mMaxElevation = computeMaxElevation();
}

LatLng Tile::latlng(Offsets pos)  const {
  float latitude = mMaxLat - (mMaxLat - mMinLat) * (((float) pos.y()) / (mHeight - 1));
  float longitude = mMinLng + (mMaxLng - mMinLng) * (((float) pos.x()) / (mWidth - 1));
  return LatLng(latitude, longitude);
}

Offsets Tile::toOffsets(float latitude, float longitude) const {
  // Samples are edge-centered; OK to go half a pixel outside a corner
  int x = (int) ((longitude - mMinLng) * (mWidth - 1) + 0.5);
  int y = (int) ((mMaxLat - latitude) * (mHeight - 1) + 0.5);

  return Offsets(x, y);
}

Elevation Tile::computeMaxElevation() const {
  Elevation max_elevation = 0;
  for (int i = 0; i < mWidth * mHeight; ++i) {
    Elevation elev = mSamples[i];
    max_elevation = std::max(max_elevation, elev);
  }

  return max_elevation;
}
