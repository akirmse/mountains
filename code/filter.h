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

#ifndef _FILTER_H_
#define _FILTER_H_

/*
 * Stores one or more polygonal regions and can determine if a point is inside.
 *
 * In general this does not support wrapping around the antimeridian.  However,
 * the specific case of the polygon extending a short way east of the antimeridian
 * can be handled by calling wrapAntimeridian(), and then testing points
 * whose longitudes have been adjusted to > 180 degrees.
 */

#include "latlng.h"

#include <string>
#include <vector>

class Filter {
public:
  Filter();
  
  bool addPolygonsFromKml(const std::string &filename);

  bool isPointInside(const LatLng &latlng) const;

  // Add 360 to the longitude of (presumably negative) longitudes <
  // wrapLongitude in the polygons.  This allows the polygons to cross
  // the antimeridian in a primitive way.
  void setWrapLongitude(double wrapLongitude);
  
  // Determines whether given rectangle intersects any polygon.
  bool intersects(double minLat, double maxLat, double minLng, double maxLng) const;

  // Sets sw and ne to northwest and southeast corners of bounds.
  void getBounds(LatLng *sw, LatLng *ne) const;
  
private:
  std::vector<std::vector<LatLng>> mPolygons;
  double mWrapLongitude;

  // Returns true if line segment p0-p1 intersects with segment p2-p3.
  bool segmentsIntersect(double p0_x, double p0_y, double p1_x, double p1_y,
                         double p2_x, double p2_y, double p3_x, double p3_y) const;  
};

#endif  // _FILTER_H_
