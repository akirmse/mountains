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

#include "coordinate_system.h"

CoordinateSystem::CoordinateSystem(float minLat, float minLng, float maxLat, float maxLng,
                                   int pixelsPerDegreeLat, int pixelsPerDegreeLng) {
  mMinLatitude = minLat;
  mMinLongitude = minLng;
  mMaxLatitude = maxLat;
  mMaxLongitude = maxLng;
  mPixelsPerDegreeLatitude = pixelsPerDegreeLat;
  mPixelsPerDegreeLongitude = pixelsPerDegreeLng;
}

bool CoordinateSystem::compatibleWith(const CoordinateSystem &that) const {
  return mPixelsPerDegreeLatitude == that.mPixelsPerDegreeLatitude &&
    mPixelsPerDegreeLongitude == that.mPixelsPerDegreeLongitude;
}

LatLng CoordinateSystem::getLatLng(Offsets offsets) const {
  // Positive y is south
  float latitude = mMaxLatitude -
    ((float) offsets.y()) / mPixelsPerDegreeLatitude;
  float longitude = mMinLongitude +
    ((float) offsets.x()) / mPixelsPerDegreeLongitude;
  return LatLng(latitude, longitude);
}

Offsets CoordinateSystem::offsetsTo(const CoordinateSystem &that) {
  int dx = static_cast<int>((mMinLongitude - that.mMinLongitude) * mPixelsPerDegreeLongitude);
  int dy = static_cast<int>((that.mMaxLatitude - mMaxLatitude) * mPixelsPerDegreeLatitude);
  return Offsets(dx, dy);
}
