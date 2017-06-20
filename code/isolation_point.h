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


#ifndef _ISOLATION_POINT_H__
#define _ISOLATION_POINT_H__

#include "point.h"
#include "latlng.h"

// Point from the isolation calculation
class IsolationPoint : public Point {
public:
  IsolationPoint(const LatLng &location,
                 int elevation,
                 float isolation,
                 const LatLng &isolationLimitPoint);
  IsolationPoint(const IsolationPoint &that)
      : Point(that) {
    *this = that;
  }
  
  virtual void operator=(const IsolationPoint &that);
  
  int elevation() const;
  float isolation() const;
  const LatLng &isolationLimitPoint() const;
  
private:
  int mElevation;
  float mIsolation;

  LatLng mIsolationLimitPoint;
};

#endif  // ifndef _PEAKBAGGER_POINT_H__
