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

#include "utm_coordinate_system.h"
#include "easylogging++.h"
#include "util.h"
#include "utm.h"

#include <cassert>
#include <vector>

using std::string;
using std::vector;

UtmCoordinateSystem::UtmCoordinateSystem(int zone, int minX, int minY, int maxX, int maxY,
                                         double metersPerSample) {
  mZone = zone;
  mMinX = minX;
  mMinY = minY;
  mMaxX = maxX;
  mMaxY = maxY;
  mMetersPerSample = metersPerSample;

  assert((mZone > 0 && mZone <= 60) && "UTM zone out of range");
}

UtmCoordinateSystem::~UtmCoordinateSystem() {
}

CoordinateSystem *UtmCoordinateSystem::clone() const {
  return new UtmCoordinateSystem(mZone, mMinX, mMinY, mMaxX, mMaxY, mMetersPerSample);
}

bool UtmCoordinateSystem::compatibleWith(const CoordinateSystem &that) const {
  // Type must agree
  auto other = dynamic_cast<const UtmCoordinateSystem *>(&that);
  if (other == nullptr) {
    return false;
  }

  if (mZone != other->mZone) {
    return false;
  }
  
  return mMetersPerSample == other->mMetersPerSample;
}

LatLng UtmCoordinateSystem::getLatLng(Offsets offsets) const {
  auto x = mMinX + offsets.x() * mMetersPerSample;
  // Positive y is south
  auto y = mMaxY - offsets.y() * mMetersPerSample;

  // TODO: Support southern hemisphere
  char zone[10];
  snprintf(zone, sizeof(zone), "%dQ", mZone);
  double lat, lng;
  UTM::UTMtoLL(y, x, zone, lat, lng);
  
  return LatLng(lat, lng);
}

Offsets UtmCoordinateSystem::offsetsTo(const CoordinateSystem &that) {
  // Type must agree
  auto other = dynamic_cast<const UtmCoordinateSystem *>(&that);
  if (other == nullptr) {
    assert(false && "offsetsTo can only operate on CoordinateSystems of the same type");
  }

  int dx = static_cast<int>((mMinX - other->mMinX) * mMetersPerSample);
  int dy = static_cast<int>((other->mMaxY - mMaxY) * mMetersPerSample);
  return Offsets(dx, dy);
}

std::unique_ptr<CoordinateSystem> UtmCoordinateSystem::mergeWith(
  const CoordinateSystem &that) const {
  // Type must agree
  auto other = dynamic_cast<const UtmCoordinateSystem *>(&that);
  if (other == nullptr) {
    assert(false && "mergeWith can only operate on CoordinateSystems of the same type");
  }

  return std::make_unique<UtmCoordinateSystem>(
    mZone,
    std::min(mMinX, other->mMinX),
    std::min(mMinY, other->mMinY),
    std::max(mMaxX, other->mMaxX),
    std::max(mMaxY, other->mMaxY),
    mMetersPerSample
    );
}

int UtmCoordinateSystem::samplesAroundEquator() const {
  return static_cast<int>(60 * 666000 / mMetersPerSample);
}

string UtmCoordinateSystem::toString() const {
  char str[200];
  snprintf(str, sizeof(str), "U,%d,%d,%d,%d,%d,%f",
           mZone, mMinX, mMinY, mMaxX, mMaxY, mMetersPerSample);
  return string(str);
}

CoordinateSystem *UtmCoordinateSystem::fromString(const string &str) {
  vector<string> elements;
  split(str, ',', elements);

  if (elements.size() < 7 || elements[0] != "U") {
    return nullptr;
  }

  int zone = stoi(elements[1]);
  int minX = stoi(elements[2]);
  int minY = stoi(elements[3]);
  int maxX = stoi(elements[4]);
  int maxY = stoi(elements[5]);
  float metersPerSample = stof(elements[6]);
  VLOG(2) << "Got min x/y " << minX << " " << minY;

  if (metersPerSample <= 0) {
    LOG(ERROR) << "Invalid sample counts in coordinate system";
    return nullptr;
  }

  return new UtmCoordinateSystem(zone, minX, minY, maxX, maxY, metersPerSample);
}
