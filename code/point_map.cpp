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

#include "point_map.h"
#include "easylogging++.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

PointMap::PointMap() {
  for (int i = 0; i < 180; ++i) {
    for (int j = 0; j < 360; ++j) {
      mBuckets[i][j] = nullptr;
    }
  }
}

PointMap::~PointMap() {
  for (int i = 0; i < 180; ++i) {
    for (int j = 0; j < 360; ++j) {
      delete mBuckets[i][j];
    }
  }
}

void PointMap::insert(const LatLng *point) {
  // We want a peak just on the left or top edge to be included with
  // the tile to its right or bottom, respectively.
  // TODO: This epsilon value depends on the pixels per tile; it's half a pixel width.  Need to get from tile?
  const float epsilon = 1.0f / 1201 / 2;

  int lat = (int) floor(point->latitude() - epsilon);
  int lng = (int) floor(point->longitude() + epsilon);

  // Peakbagger has a peak on the antimeridian
  if (lng >= 180) {
    VLOG(1) << "Skipping peak on antimeridian at " << lat << ", " << lng;
    return;
  }
  
  assert(lat >= -90);
  assert(lat < 90);
  assert(lng >= -180);
  assert(lng < 180);

  lat += 90;
  lng += 180;

  if (mBuckets[lat][lng] == nullptr) {
    mBuckets[lat][lng] = new Bucket();
  }

  mBuckets[lat][lng]->push_back(point);
}

PointMap::Bucket *PointMap::lookup(float lat, float lng) const {
  if (lat < -90 || lat >= 90 || lng < -180 || lng >= 180) {
    return nullptr;
  }

  lat += 90;
  lng += 180;
  
  return mBuckets[(int) lat][(int) lng];
}
