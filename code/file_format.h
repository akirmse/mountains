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

#ifndef _FILE_FORMAT_H_
#define _FILE_FORMAT_H_

#include <string>

// Defines the types of input tiles we can read, and their properties.
class FileFormat {
public:
  enum class Value {
    HGT,  // SRTM (90m, 3 arcsecond)
    NED19,     // FLT file containing NED 1/9 arcsecond data
    NED13_ZIP, // ZIP file containing FLT NED 1/3 arcsecond data
    NED1_ZIP,  // ZIP file containing FLT NED 1 arcsecond data
    THREEDEP_1M,  // FLT file containing one-meter LIDAR from 3D Elevation Program (3DEP)
    GLO30,  // Copernicus GLO-30 30m data
  };

  FileFormat() = default;
  constexpr FileFormat(Value v) : mValue(v) {}

  Value value() const { return mValue; }

  // Return the number of samples in one row or column of the file format,
  // including any border samples.
  int samplesAcross() const;

  // Return the degrees in lat or lng covered by one tile.
  // Note that this is the logical value (1 degree, 0.25 degree), not necessarily
  // the precise value covered, including border samples.
  float degreesAcross() const;

  // Does this format use UTM coordinates rather than lat/lng?
  bool isUtm() const;
  
  // Return a FileFormat object for the given human-readable string,
  // or nullptr if none.
  static FileFormat *fromName(const std::string &name);
  
private:
  Value mValue;
};

#endif  // _FILE_FORMAT_H_
