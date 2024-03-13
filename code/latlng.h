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

#include <vector>

class LatLng {
public:

  LatLng() {
    mLatitude = 0;
    mLongitude = 0;
  }
  
  LatLng(double lat, double lng) {
    mLatitude = lat;
    mLongitude = lng;
  }

  LatLng(const LatLng &other) {
    *this = other;
  }

  void operator=(const LatLng &other) {
    mLatitude = other.mLatitude;
    mLongitude = other.mLongitude;
  }

  double latitude() const { return mLatitude; }
  double longitude() const { return mLongitude; }

  // Distance in meters
  double distance(const LatLng &other) const;

  // Distance in meters using a more accurate but slower calculation
  double distanceEllipsoid(const LatLng &other) const;

  // Initial bearing to point in radians
  double bearingTo(const LatLng &that) const;

  /**
	 * <p>Computes the bounding coordinates of all points on the surface
	 * of a sphere that have a great circle distance to the point represented
	 * by this GeoLocation instance that is less or equal to the distance
	 * argument.</p>
	 * <p>For more information about the formulae used in this method visit
	 * <a href="http://JanMatuschek.de/LatitudeLongitudeBoundingCoordinates">
	 * http://JanMatuschek.de/LatitudeLongitudeBoundingCoordinates</a>.</p>
	 * @param distance the distance from the point represented by this
	 * GeoLocation instance. Must me measured in the same unit as the radius
	 * argument.
	 * @param radius the radius of the sphere, e.g. the average radius for a
	 * spherical approximation of the figure of the Earth is approximately
	 * 6371.01 kilometers.
	 * @return an array of two GeoLocation objects such that:<ul>
	 * <li>The latitude of any point within the specified distance is greater
	 * or equal to the latitude of the first array element and smaller or
	 * equal to the latitude of the second array element.</li>
	 * <li>If the longitude of the first array element is smaller or equal to
	 * the longitude of the second element, then
	 * the longitude of any point within the specified distance is greater
	 * or equal to the longitude of the first array element and smaller or
	 * equal to the longitude of the second array element.</li>
	 * <li>If the longitude of the first array element is greater than the
	 * longitude of the second element (this is the case if the 180th
	 * meridian is within the distance), then
	 * the longitude of any point within the specified distance is greater
	 * or equal to the longitude of the first array element
	 * <strong>or</strong> smaller or equal to the longitude of the second
	 * array element.</li>
	 * </ul>
	 */   
  std::vector<LatLng> GetBoundingBoxForCap(double distance_meters) const;
  
private:
  double mLatitude, mLongitude;
};

#endif  // _LATLNG_H_
