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


#ifndef _PROMINENCE_POINT_H__
#define _PROMINENCE_POINT_H__

#include "point.h"
#include "latlng.h"

// Point from the prominence calculation
class ProminencePoint : public Point {
public:
  ProminencePoint(const LatLng &location,
                  int elevation,
                  int prominence,
                  const LatLng &keySaddle);
  ProminencePoint(const ProminencePoint &that)
      : Point(that) {
    *this = that;
  }
  
  virtual void operator=(const ProminencePoint &that);
  
  int elevation() const;
  int prominence() const;
  const LatLng &keySaddle() const;
  
private:
  int mElevation;
  int mProminence;

  LatLng mKeySaddle;
};

#endif  // ifndef _PROMINENCE_POINT_H__
