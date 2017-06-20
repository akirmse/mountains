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

#include "filter.h"
#include "util.h"

#include <fstream>

using std::string;
using std::vector;

Filter::Filter()
    : mWrapLongitude(-180) {
}

bool Filter::addPolygonFromKml(const string &filename) {
  if (!fileExists(filename)) {
    printf("Can't find polygon file %s\n", filename.c_str());
    return false;
  }

  std::ifstream file(filename);

  bool inCoordinates = false;
  string line;
  while (file.good()) {
    std::getline(file, line);

    if (line.find("<coordinates>") != string::npos) {
      inCoordinates = true;
    } else if (line.find("</coordinates>") != string::npos) {
      inCoordinates = false;
    } else if (inCoordinates) {
      vector<string> pointText;
      vector<string> elements;
      split(line, ' ', pointText);
      for (const string &eachPoint : pointText) {
        split(eachPoint, ',', elements);
        float lat = stof(elements[1]);  // note order in KML
        float lng = stof(elements[0]);
        mPolygon.push_back(Point(lat, lng));
      }
    }
  }

  printf("Read polygon with %d points\n", (int) mPolygon.size());
  
  return true;
}

// Polygon must be closed, i.e. first point == last point.
bool Filter::isPointInside(const Point &point) const {
  if (mPolygon.empty()) {
    return true;
  }
  
  bool inside = false;
  float testx = point.longitude();
  float testy = point.latitude();

  // Allow wrapping around antimeridian
  if (testx < mWrapLongitude) {
    testx += 360;
  }
  
  // Horizontal ray cast, check parity of # of polygon intersections.
  // See http://stackoverflow.com/questions/11716268/point-in-polygon-algorithm 
  for (int i = 0, j = mPolygon.size() - 1; i < (int) mPolygon.size(); j = i++) {
    if ( ((mPolygon[i].latitude() > testy) != (mPolygon[j].latitude() > testy)) &&
         (testx < (mPolygon[j].longitude() - mPolygon[i].longitude()) *
          (testy - mPolygon[i].latitude()) / (mPolygon[j].latitude()-mPolygon[i].latitude()) + mPolygon[i].longitude()) )
      inside = !inside;
  }
  return inside;
}

void Filter::setWrapLongitude(float wrapLongitude) {
  mWrapLongitude = wrapLongitude;
  vector<Point> newPoints;
  for (const Point &p : mPolygon) {
    if (p.longitude() < wrapLongitude) {
      newPoints.push_back(Point(p.latitude(), p.longitude() + 360));
    } else {
      newPoints.push_back(p);
    }
  }
  mPolygon = newPoints;
}

bool Filter::intersects(float minLat, float maxLat, float minLng, float maxLng) const {
  // If any corner of the rectangle is inside, there is an intersection
  Point p1(minLat, minLng);
  Point p2(minLat, maxLng);
  Point p3(maxLat, maxLng);
  Point p4(maxLat, minLng);
  if (isPointInside(p1) || isPointInside(p2) || isPointInside(p3) || isPointInside(p4)) {
    return true;
  }

  // Try intersecting the four sides of the rectangle with all edges of the polygon
  Point points[4] = { p1, p2, p3, p4};
  for (int i = 0; i < 4; ++i) {
    const Point &point1 = points[i];
    const Point &point2 = points[(i + 1) % 4];
    for (int j = 0, k = mPolygon.size() - 1; j < (int) mPolygon.size(); k = j++) {
      if (segmentsIntersect(point1.longitude(), point1.latitude(),
                            point2.longitude(), point2.latitude(),
                            mPolygon[j].longitude(), mPolygon[j].latitude(),
                            mPolygon[k].longitude(), mPolygon[k].latitude())) {
        return true;
      }
    }
  }
  return false;
}


// Returns true if the lines intersect, otherwise false.
bool Filter::segmentsIntersect(float p0_x, float p0_y, float p1_x, float p1_y,
                               float p2_x, float p2_y, float p3_x, float p3_y) const {
  // See http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
  float s1_x, s1_y, s2_x, s2_y;
  s1_x = p1_x - p0_x;     s1_y = p1_y - p0_y;
  s2_x = p3_x - p2_x;     s2_y = p3_y - p2_y;

  float s, t;
  s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
  t = ( s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

  if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
    return true;
  }

  return false;
}
      

