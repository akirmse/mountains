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

#include "easylogging++.h"
#include "filter.h"
#include "util.h"

#include <fstream>


using std::string;
using std::vector;

Filter::Filter()
    : mWrapLongitude(-180) {
}

bool Filter::addPolygonsFromKml(const string &filename) {
  if (!fileExists(filename)) {
    printf("Can't find polygon file %s\n", filename.c_str());
    return false;
  }

  std::ifstream file(filename);

  bool inCoordinates = false;
  string line;
  vector<LatLng> polygon;
  while (file.good()) {
    std::getline(file, line);

    // Try to parse coordinates all on one line, or one line at a time
    const char *coordinates_tag = "<coordinates>";
    auto pos = line.find(coordinates_tag);
    if (pos != string::npos) {
      inCoordinates = true;
      line = line.substr(pos + strlen(coordinates_tag));
    }

    bool endFound = false;
    if (inCoordinates) {
      // Look for </coordinates> on same line
      pos = line.find("</coordinates>");
      auto coordsStr = line;
      if (pos != string::npos) {
        coordsStr = line.substr(0, pos);
        endFound = true;
      } 
      vector<string> pointText;
      vector<string> elements;
      split(coordsStr, ' ', pointText);
      for (const string &eachPoint : pointText) {
        split(eachPoint, ',', elements);
        if (elements.size() >= 2) {
          double lat = stod(elements[1]);  // note order in KML
          double lng = stod(elements[0]);
          polygon.push_back(LatLng(lat, lng));
        }
      }
    }

    if (endFound || line.find("</coordinates>") != string::npos) {
      inCoordinates = false;
      mPolygons.push_back(polygon);
      polygon.clear();
    } 
  }

  size_t numPoints = 0;
  for (auto &polygon : mPolygons) {
    numPoints += polygon.size();
  }

  LOG(INFO) << "Read " << mPolygons.size() << " polygons with " << numPoints << " points";
  
  return true;
}

// Polygon must be closed, i.e. first point == last point.
bool Filter::isPointInside(const LatLng &latlng) const {
  if (mPolygons.empty()) {
    return true;
  }

  for (auto &polygon : mPolygons) {
    if (polygon.empty()) {
      continue;
    }
    
    bool inside = false;
    auto testx = latlng.longitude();
    auto testy = latlng.latitude();
    
    // Allow wrapping around antimeridian
    if (testx < mWrapLongitude) {
      testx += 360;
    }
    
    // Horizontal ray cast, check parity of # of polygon intersections.
    // See http://stackoverflow.com/questions/11716268/point-in-polygon-algorithm 
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
      if ( ((polygon[i].latitude() > testy) != (polygon[j].latitude() > testy)) &&
           (testx < (polygon[j].longitude() - polygon[i].longitude()) *
            (testy - polygon[i].latitude()) / (polygon[j].latitude()-polygon[i].latitude()) + polygon[i].longitude()) )
        inside = !inside;
    }

    if (inside) {
      return true;
    }
  }
  
  return false;
}

void Filter::setWrapLongitude(double wrapLongitude) {
  mWrapLongitude = wrapLongitude;
  vector<vector<LatLng>> newPolygons;
  for (auto &polygon : mPolygons) {
    vector<LatLng> newPoints;
    for (const LatLng &p : polygon) {
      if (p.longitude() < wrapLongitude) {
        newPoints.push_back(LatLng(p.latitude(), p.longitude() + 360));
      } else {
        newPoints.push_back(p);
      }
    }
    newPolygons.push_back(newPoints);
  }
  mPolygons = newPolygons;
}

bool Filter::intersects(double minLat, double maxLat, double minLng, double maxLng) const {
  // If any corner of the rectangle is inside, there is an intersection
  LatLng p1(minLat, minLng);
  LatLng p2(minLat, maxLng);
  LatLng p3(maxLat, maxLng);
  LatLng p4(maxLat, minLng);
  if (isPointInside(p1) || isPointInside(p2) || isPointInside(p3) || isPointInside(p4)) {
    return true;
  }

  for (auto &polygon : mPolygons) {
    // Try intersecting the four sides of the rectangle with all edges of the polygon
    LatLng points[4] = { p1, p2, p3, p4};
    for (int i = 0; i < 4; ++i) {
      const LatLng &point1 = points[i];
      const LatLng &point2 = points[(i + 1) % 4];
      for (size_t j = 0, k = polygon.size() - 1; j < polygon.size(); k = j++) {
        if (segmentsIntersect(point1.longitude(), point1.latitude(),
                              point2.longitude(), point2.latitude(),
                              polygon[j].longitude(), polygon[j].latitude(),
                              polygon[k].longitude(), polygon[k].latitude())) {
          return true;
        }
      }
    }
    
    // Is any point of polygon inside the rectangle?
    for (auto &point : polygon) {
      if (point.latitude() >= minLat &&
          point.latitude() <= maxLat &&
          point.longitude() >= minLng &&
          point.longitude() <= maxLng) {
        return true;
      }
    }
  }
  
  return false;
}

void Filter::getBounds(LatLng *sw, LatLng *ne) const {
  if (mPolygons.empty()) {
    return;
  }

  bool anyPoints = false;
  
  double min_latitude = 999;
  double max_latitude = -999;
  double min_longitude = 999;
  double max_longitude = -999;
  
  for (auto &polygon : mPolygons) {
    if (polygon.empty()) {
      continue;
    }

    for (const LatLng &point : polygon) {
      min_latitude = std::min(min_latitude, point.latitude());
      max_latitude = std::max(max_latitude, point.latitude());
      min_longitude = std::min(min_longitude, point.longitude());
      max_longitude = std::max(max_longitude, point.longitude());
    }
    
    *sw = LatLng(min_latitude, min_longitude);
    *ne = LatLng(max_latitude, max_longitude);
  }
}

// Returns true if the lines intersect, otherwise false.
bool Filter::segmentsIntersect(double p0_x, double p0_y, double p1_x, double p1_y,
                               double p2_x, double p2_y, double p3_x, double p3_y) const {
  // See http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
  double s1_x, s1_y, s2_x, s2_y;
  s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
  s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

  double s, t;
  s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
  t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

  if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
    return true;
  }

  return false;
}
      

