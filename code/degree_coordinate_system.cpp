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

#include "degree_coordinate_system.h"
#include "easylogging++.h"
#include "util.h"

#include <cassert>
#include <cmath>
#include <vector>

using std::string;
using std::vector;

DegreeCoordinateSystem::DegreeCoordinateSystem(double minLat, double minLng, double maxLat, double maxLng,
                                               int pixelsPerDegreeLat, int pixelsPerDegreeLng) {
  mMinLatitude = minLat;
  mMinLongitude = minLng;
  mMaxLatitude = maxLat;
  mMaxLongitude = maxLng;
  mSamplesPerDegreeLatitude = pixelsPerDegreeLat;
  mSamplesPerDegreeLongitude = pixelsPerDegreeLng;
}

DegreeCoordinateSystem::~DegreeCoordinateSystem() {
}

CoordinateSystem *DegreeCoordinateSystem::clone() const {
  return new DegreeCoordinateSystem(mMinLatitude, mMinLongitude,
                                    mMaxLatitude, mMaxLongitude,
                                    mSamplesPerDegreeLatitude,
                                    mSamplesPerDegreeLongitude);
}

bool DegreeCoordinateSystem::compatibleWith(const CoordinateSystem &that) const {
  // Type must agree
  auto other = dynamic_cast<const DegreeCoordinateSystem *>(&that);
  if (other == nullptr) {
    return false;
  }
  
  return mSamplesPerDegreeLatitude == other->mSamplesPerDegreeLatitude &&
    mSamplesPerDegreeLongitude == other->mSamplesPerDegreeLongitude;
}

LatLng DegreeCoordinateSystem::getLatLng(Offsets offsets) const {
  // Positive y is south
  double latitude = mMaxLatitude -
    ((double) offsets.y()) / mSamplesPerDegreeLatitude;
  double longitude = mMinLongitude +
    ((double) offsets.x()) / mSamplesPerDegreeLongitude;
  return LatLng(latitude, longitude);
}

Offsets DegreeCoordinateSystem::offsetsTo(const CoordinateSystem &that) {
  // Type must agree
  auto other = dynamic_cast<const DegreeCoordinateSystem *>(&that);
  if (other == nullptr) {
    assert(false && "offsetsTo can only operate on CoordinateSystems of the same type");
  }

  int dx = static_cast<int>(std::round((mMinLongitude - other->mMinLongitude) * mSamplesPerDegreeLongitude));
  int dy = static_cast<int>(std::round((other->mMaxLatitude - mMaxLatitude) * mSamplesPerDegreeLatitude));
  return Offsets(dx, dy);
}

std::unique_ptr<CoordinateSystem> DegreeCoordinateSystem::mergeWith(
  const CoordinateSystem &that) const {
  // Type must agree
  auto other = dynamic_cast<const DegreeCoordinateSystem *>(&that);
  if (other == nullptr) {
    assert(false && "mergeWith can only operate on CoordinateSystems of the same type");
  }

  return std::make_unique<DegreeCoordinateSystem>(
    std::min(mMinLatitude, other->mMinLatitude),
    std::min(mMinLongitude, other->mMinLongitude),
    std::max(mMaxLatitude, other->mMaxLatitude),
    std::max(mMaxLongitude, other->mMaxLongitude),
    mSamplesPerDegreeLatitude,
    mSamplesPerDegreeLongitude
    );
}

int DegreeCoordinateSystem::samplesAroundEquator() const {
  return 360 * mSamplesPerDegreeLongitude;
}

string DegreeCoordinateSystem::toString() const {
  char str[200];
  snprintf(str, sizeof(str), "G,%f,%f,%d,%d,%f,%f",
           mMinLatitude, mMinLongitude,
           mSamplesPerDegreeLatitude,
           mSamplesPerDegreeLongitude,
           mMaxLatitude,
           mMaxLongitude);
  return string(str);
}

CoordinateSystem *DegreeCoordinateSystem::fromString(const string &str) {
  double minLat = 0, minLng = 0, maxLat = 0, maxLng = 0;
  int samplesPerLat = 0, samplesPerLng = 0;

  vector<string> elements;
  split(str, ',', elements);

  if (elements.size() < 5 || elements[0] != "G") {
    return nullptr;
  }
  
  minLat = stod(elements[1]);
  minLng = stod(elements[2]);
  samplesPerLat = stoi(elements[3]);
  samplesPerLng = stoi(elements[4]);
  // Max lat/lng was added later for non-1x1 tile support
  if (elements.size() >= 7) {
    maxLat = stod(elements[5]);
    maxLng = stod(elements[6]);
  } else {
    // Assume 1x1 tile for old DVT files
    maxLat = minLat + 1;
    maxLng = minLng + 1;
  }
  VLOG(2) << "Got min lat/lng " << minLat << " " << minLng;

  if (samplesPerLat <= 0 || samplesPerLng <= 0) {
    LOG(ERROR) << "Invalid sample counts in coordinate system";
    return nullptr;
  }

  return new DegreeCoordinateSystem(minLat, minLng, maxLat, maxLng,
                                    samplesPerLat, samplesPerLng);
}
