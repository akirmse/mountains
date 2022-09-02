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

#ifndef _COORDINATE_SYSTEM_H_
#define _COORDINATE_SYSTEM_H_

// Helps convert a location (like an Offsets) into a point on the Earth.

#include "primitives.h"
#include "latlng.h"

class CoordinateSystem {
public:

  CoordinateSystem(float minLat, float minLng, float maxLat, float maxLng,
                   int pixelsPerDegreeLat, int pixelsPerDegreeLng);
  
  // true if the two systems have the same number of pixels per degree
  bool compatibleWith(const CoordinateSystem &that) const;

  LatLng getLatLng(Offsets offsets) const;

  // Return ths offsets to go from our coordinate system to the given one.
  Offsets offsetsTo(const CoordinateSystem &that);
  
  float minLatitude() const { return mMinLatitude; }
  float minLongitude() const { return mMinLongitude; }
  float maxLatitude() const { return mMaxLatitude; }
  float maxLongitude() const { return mMaxLongitude; }
  int pixelsPerDegreeLatitude() const { return mPixelsPerDegreeLatitude; }
  int pixelsPerDegreeLongitude() const { return mPixelsPerDegreeLongitude; }
  
private:
  float mMinLatitude;
  float mMinLongitude;
  float mMaxLatitude;
  float mMaxLongitude;
  int mPixelsPerDegreeLatitude;
  int mPixelsPerDegreeLongitude;
};
  

#endif  // _COORDINATE_SYSTEM_H_
