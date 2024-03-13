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

#ifndef _UTM_COORDINATE_SYSTEM_H_
#define _UTM_COORDINATE_SYSTEM_H_

#include "coordinate_system.h"

// A coordinate system where the corners are specified as UTM northing and easting,
// and samples are a constant number of meters.

class UtmCoordinateSystem : public CoordinateSystem {
public:

  UtmCoordinateSystem(int zone, int minX, int minY, int maxX, int maxY,
                      double metersPerSample);
  virtual ~UtmCoordinateSystem();

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
  // Longitude zone (1-60)
  int mZone;
  // Coordinates of corners in meters
  int mMinX;
  int mMinY;
  int mMaxX;
  int mMaxY;
  double mMetersPerSample;
};
  
#endif  // _UTM_COORDINATE_SYSTEM_H_

