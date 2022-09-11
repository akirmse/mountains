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

#ifndef _COORDINATE_SYSTEM_H_
#define _COORDINATE_SYSTEM_H_

// Represents the area that a tile covers on the Earth.

#include "primitives.h"
#include "latlng.h"

#include <memory>
#include <string>

class CoordinateSystem {
public:
  virtual ~CoordinateSystem() = default;
  
  virtual CoordinateSystem *clone() const = 0;
  
  virtual LatLng getLatLng(Offsets offsets) const = 0;

  // true if the two systems have the same number of pixels per degree
  virtual bool compatibleWith(const CoordinateSystem &that) const = 0;

  // Return a new CoordinateSystem that's the result of merging this one
  // and another one, by expanding the bounds.
  virtual std::unique_ptr<CoordinateSystem> mergeWith(const CoordinateSystem &that) const = 0;

  // Return the offsets to go from our coordinate system to the given one.
  virtual Offsets offsetsTo(const CoordinateSystem &that) = 0;

  // Return the number of samples required to go around the equator
  virtual int samplesAroundEquator() const = 0;
  
  // Return a string completely describing the CoordinateSystem
  virtual std::string toString() const = 0;

  // Return a new CoordinateSystem object from the given string, which must
  // have been previously returned by toString(),
  // or nullptr if not possible.
  static CoordinateSystem *fromString(const std::string &str);
};

#endif  // _COORDINATE_SYSTEM_H_
