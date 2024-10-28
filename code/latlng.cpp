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

#include "latlng.h"

#include "math_util.h"
#include <assert.h>
#include <math.h>
#include <algorithm>

static const double kEarthRadiusMeters = 6371.01 * 1000.0;
static const double kMinLatRadians = -M_PI / 2;
static const double kMaxLatRadians = M_PI / 2;
static const double kMinLngRadians = -M_PI;
static const double kMaxLngRadians = M_PI;

double LatLng::distance(const LatLng &other) const {
  // See http://www.movable-type.co.uk/scripts/latlong.html
  auto lat1 = degToRad(mLatitude);
  auto lat2 = degToRad(other.latitude());
  auto lng1 = degToRad(mLongitude);
  auto lng2 = degToRad(other.longitude());

  auto deltaLat = lat1 - lat2;
  auto deltaLng = lng1 - lng2;
  auto a = sin(deltaLat / 2);
  a = a * a;
  auto term = sin(deltaLng / 2);
  a += term * term * cos(lat1) * cos(lat2);
  auto c = 2 * atan2(sqrt(a), sqrt(1-a));
  
  return c * kEarthRadiusMeters;
}

double LatLng::distanceEllipsoid(const LatLng &other) const {
  // Implementation from Greg Slayden's code; source unknown
  auto lat1 = degToRad(mLatitude);
  auto lat2 = degToRad(other.latitude());
  auto lng1 = degToRad(mLongitude);
  auto lng2 = degToRad(other.longitude());

  // WGS84 ellipsoid
  double majorAxisKm = 6378.137;
  double flat = 1 / 298.257223563;
  
  auto sinF = sin((lat1 + lat2) / 2);
  auto cosF = cos((lat1 + lat2) / 2);
  auto sinG = sin((lat1 - lat2) / 2);
  auto cosG = cos((lat1 - lat2) / 2);
  auto sinL = sin((lng1 - lng2) / 2);
  auto cosL = cos((lng1 - lng2) / 2);

  auto s = sinG * sinG * cosL * cosL + cosF * cosF * sinL * sinL;
  auto c = cosG * cosG * cosL * cosL + sinF * sinF * sinL * sinL;
  auto w = atan2(sqrt(s), sqrt(c));
  auto r = sqrt(s * c) / w;
  auto distance = ((2 * w * majorAxisKm) *
                   (1 + flat * ((3 * r - 1) / (2 * c)) * (sinF * sinF * cosG * cosG) -
                    (flat * ((3 * r + 1) / (2 * s)) * cosF * cosF * sinG * sinG)));
  return distance * 1000;  // meters
}

double LatLng::bearingTo(const LatLng &other) const {
  // See http://www.movable-type.co.uk/scripts/latlong.html
  auto lat1 = degToRad(mLatitude);
  auto lat2 = degToRad(other.latitude());
  auto lng1 = degToRad(mLongitude);
  auto lng2 = degToRad(other.longitude());

  auto deltaLng = lng1 - lng2;

  auto term1 = sin(deltaLng) * cos(lat2);
  auto term2 = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(deltaLng);
  return atan2(term1, term2);
}

std::vector<LatLng> LatLng::GetBoundingBoxForCap(double distance_meters) const {
   assert(distance_meters >= 0);
   
   // angular distance in radians on a great circle
   auto radDist = distance_meters / kEarthRadiusMeters;
   auto radLat = degToRad(mLatitude);
   auto radLon = degToRad(mLongitude);
   auto minLat = radLat - radDist;
   auto maxLat = radLat + radDist;
   
   double minLon, maxLon;
   if (minLat > kMinLatRadians && maxLat < kMaxLatRadians) {
      auto deltaLon = asin(sin(radDist) / cos(radLat));
      minLon = radLon - deltaLon;
      if (minLon < kMinLngRadians) minLon += 2 * M_PI;
      maxLon = radLon + deltaLon;
      if (maxLon > kMaxLngRadians) maxLon -= 2 * M_PI;
   } else {
      // a pole is within the distance
      minLat = std::max(minLat, kMinLatRadians);
      maxLat = std::min(maxLat, kMaxLatRadians);
      minLon = kMinLngRadians;
      maxLon = kMaxLngRadians;
   }

   std::vector<LatLng> box;
   box.push_back(LatLng(radToDeg(minLat), radToDeg(minLon)));
   box.push_back(LatLng(radToDeg(maxLat), radToDeg(maxLon)));
   return box;
}
