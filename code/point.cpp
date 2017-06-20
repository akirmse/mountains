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

#include "point.h"
#include <assert.h>
#include <math.h>
#include <algorithm>

static const float kEarthRadiusMeters = 6371.01 * 1000.0;
static const float kMinLatRadians = (float) -M_PI / 2;
static const float kMaxLatRadians = (float) M_PI / 2;
static const float kMinLngRadians = (float) -M_PI;
static const float kMaxLngRadians = (float) M_PI;

Point::Point(float latitude, float longitude)
      : mLatitude(latitude),
        mLongitude(longitude) {
}

Point::~Point() {
}

float Point::latitude() const {
   return mLatitude;
}

float Point::longitude() const {
   return mLongitude;
}

void Point::operator=(const Point &that) {
  mLatitude = that.mLatitude;
  mLongitude = that.mLongitude;
}

float Point::DistanceTo(const Point &other) const {
  // TODO: Same code is in LatLng; move to a common place
  // See http://www.movable-type.co.uk/scripts/latlong.html
  float lat1 = DegToRad(mLatitude);
  float lat2 = DegToRad(other.latitude());
  float lng1 = DegToRad(mLongitude);
  float lng2 = DegToRad(other.longitude());

  float deltaLat = lat1 - lat2;
  float deltaLng = lng1 - lng2;
  float a = sinf(deltaLat / 2);
  a = a * a;
  float term = sinf(deltaLng / 2);
  a += term * term * cosf(lat1) * cosf(lat2);
  float c = 2 * atan2(sqrtf(a), sqrtf(1-a));
  
  const float earthRadius = 6371000;
  return c * earthRadius;
}

std::vector<Point> Point::GetBoundingBoxForCap(float distance_meters) const {
   assert(distance_meters >= 0);
   
   // angular distance in radians on a great circle
   float radDist = distance_meters / kEarthRadiusMeters;
   float radLat = DegToRad(mLatitude);
   float radLon = DegToRad(mLongitude);
   float minLat = radLat - radDist;
   float maxLat = radLat + radDist;
   
   float minLon, maxLon;
   if (minLat > kMinLatRadians && maxLat < kMaxLatRadians) {
      float deltaLon = asinf(sinf(radDist) / cosf(radLat));
      minLon = radLon - deltaLon;
      if (minLon < kMinLngRadians) minLon += static_cast<float>(2 * M_PI);
      maxLon = radLon + deltaLon;
      if (maxLon > kMaxLngRadians) maxLon -= static_cast<float>(2 * M_PI);
   } else {
      // a pole is within the distance
      minLat = std::max(minLat, kMinLatRadians);
      maxLat = std::min(maxLat, kMaxLatRadians);
      minLon = kMinLngRadians;
      maxLon = kMaxLngRadians;
   }

   std::vector<Point> box;
   box.push_back(Point(RadToDeg(minLat), RadToDeg(minLon)));
   box.push_back(Point(RadToDeg(maxLat), RadToDeg(maxLon)));
   return box;
}

float Point::DegToRad(float degrees) const {
   return static_cast<float>(degrees * M_PI / 180.0);
}

float Point::RadToDeg(float radians) const {
   return static_cast<float>(radians / M_PI * 180.0);
}
