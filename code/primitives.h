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

#ifndef _PRIMITIVES_H_
#define _PRIMITIVES_H_

#include <stdint.h>

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float Elevation;
typedef int32 Coord;

// Encoded x, y offsets inside tile
class Offsets {
public:
  typedef uint64 Value;
  
  Offsets(Coord x, Coord y) {
    mValue = (((uint64) y) << (8 * sizeof(Coord))) | ((uint64) x);
  }

  explicit Offsets(Value value) {
    mValue = value;
  }

  Coord x() const {
    return mValue & 0xffffffff;
  }
  
  Coord y() const {
    return (mValue & 0xffffffff00000000) >> (8 * sizeof(Coord));
  }

  Value value() const {
    return mValue;
  }

  bool operator==(const Offsets &that) const {
    return mValue == that.mValue;
  }
  
  Offsets offsetBy(int dx, int dy) {
    return Offsets(x() + dx, y() + dy);
  }
  
private:
  Value mValue;
};


struct Peak {
  Peak(Offsets loc, Elevation elev) :
      location(loc), elevation(elev) {
  }
  
  Offsets location;
  Elevation elevation;
};

struct Saddle {
  enum class Type : unsigned char {
    // FALSE and ERROR are macros on Windows
    FALSE_SADDLE = 'f',   // Topologically a saddle, but both divides reach same peak
    PROM = 'p',    // A key saddle of a peak
    BASIN = 'b',   // A basin saddle; not a key saddle of a peak
    ERROR_SADDLE = 'e',   // Couldn't figure out what kind of saddle; indicates a bug
  };

  static Type typeFromChar(unsigned char c) {
    switch (c) {
    case 'f': return Type::FALSE_SADDLE;
    case 'p': return Type::PROM;
    case 'b': return Type::BASIN;
    default: return Type::ERROR_SADDLE;
    }
  }

  Saddle(Offsets loc, Elevation elev) :
      location(loc), elevation(elev), type(Type::PROM) {
  }
  Saddle(const Saddle &other, Type newType) :
      location(other.location), elevation(other.elevation), type(newType) {
  }
      
  Offsets location;
  Elevation elevation;
  Type type;
};

// A runoff is a point along the edge of the tile that looks like half
// a saddle: it's a point from a (possibly one-pixel) flat area along
// the edge higher than its neighbors along the edge.
//
// Half a saddle may not look like a full saddle within a single tile,
// so we store a runoff as a marker of where there may be a saddle
// when the neighboring tile is included.
//
// Runoffs cannot be removed until all of their neighboring pixels
// have been examined.  At corners of tiles, that requires looking at
// all 4 neighboring quadrants.  We need to keep track of how many
// have been examined during merges.
struct Runoff {
  Runoff(Offsets loc, Elevation elev, int filledQuads) :
      location(loc), elevation(elev), insidePeakArea(false),
      filledQuadrants(filledQuads) {
  }
  
  Offsets location;
  Elevation elevation;
  // true if this location is part of the flat area of a Peak.
  // (A Peak area touching the edge of the tile may not truly be
  // a peak, since we don't know if it's higher than what's in the
  // neighboring tile.)
  bool insidePeakArea;

  // How many neighboring quadrants have been examined.  Max 4.
  int filledQuadrants;
};

#endif  // _PRIMITIVES_H_
