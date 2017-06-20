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

#ifndef _LATLNG_H_
#define _LATLNG_H_

class LatLng {
public:

  LatLng() {
    mLatitude = 0;
    mLongitude = 0;
  }
  
  LatLng(float lat, float lng) {
    mLatitude = lat;
    mLongitude = lng;
  }

  float latitude() const { return mLatitude; }
  float longitude() const { return mLongitude; }

  // Distance in meters
  float distance(const LatLng &other) const;

  // Distance in meters using a more accurate but slower calculation
  float distanceEllipsoid(const LatLng &other) const;

  // Initial bearing to point in radians
  float bearingTo(const LatLng &that) const;
  
  LatLng(const LatLng &other) {
    *this = other;
  }

  void operator=(const LatLng &other) {
    mLatitude = other.mLatitude;
    mLongitude = other.mLongitude;
  }
  
private:
  float mLatitude, mLongitude;
};

#endif  // _LATLNG_H_
