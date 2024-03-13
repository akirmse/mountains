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

#ifndef _DEGREE_COORDINATE_SYSTEM_H_
#define _DEGREE_COORDINATE_SYSTEM_H_

#include "coordinate_system.h"

// A coordinate system where the corners are specified in lat/lng,
// and samples are assumed to be linearly spaced in lat/lng.

class DegreeCoordinateSystem : public CoordinateSystem {
public:

  DegreeCoordinateSystem(double minLat, double minLng, double maxLat, double maxLng,
                         int pixelsPerDegreeLat, int pixelsPerDegreeLng);
  virtual ~DegreeCoordinateSystem();

  virtual CoordinateSystem *clone() const;

  virtual LatLng getLatLng(Offsets offsets) const;

  // true if the two systems have the same number of pixels per degree
  virtual bool compatibleWith(const CoordinateSystem &that) const;

  virtual Offsets offsetsTo(const CoordinateSystem &that);

  virtual std::unique_ptr<CoordinateSystem> mergeWith(const CoordinateSystem &that) const;

  virtual int samplesAroundEquator() const;

  virtual std::string toString() const;

  static CoordinateSystem *fromString(const std::string &str);

private:
  double mMinLatitude;
  double mMinLongitude;
  double mMaxLatitude;
  double mMaxLongitude;
  int mSamplesPerDegreeLatitude;
  int mSamplesPerDegreeLongitude;
};
  
#endif  // _DEGREE_COORDINATE_SYSTEM_H_

