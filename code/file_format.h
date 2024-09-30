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

class CoordinateSystem;
class Tile;

// Defines the types of input tiles we can read, and their properties.
class FileFormat {
public:
  enum class Value {
    HGT,       // SRTM (90m, 3 arcsecond)
    HGT30,     // SRTM (30m, 1 arcsecond)
    NED19,     // FLT file containing NED 1/9 arcsecond data
    NED13,     // FLT file containing NED 1/3 arcsecond data
    NED13_ZIP, // ZIP file containing FLT NED 1/3 arcsecond data
    NED1_ZIP,  // ZIP file containing FLT NED 1 arcsecond data
    THREEDEP_1M,  // FLT file containing one-meter LIDAR from 3D Elevation Program (3DEP)
    GLO30,  // Copernicus GLO-30 30m data
    FABDEM, // Tree-free Copernicus GLO-30 30m data
    CUSTOM, // FLT file with customizable resolution
  };

  FileFormat() = default;
  constexpr FileFormat(Value v) : mValue(v) {}
  virtual ~FileFormat() {}

  Value value() const { return mValue; }

  // Return the number of samples in one row or column of the file format,
  // including any border samples.
  virtual int rawSamplesAcross() const;

  // Return the number of samples in a tile after its loaded and the borders
  // have been modified.
  virtual int inMemorySamplesAcross() const;
  
  // Return the degrees in lat or lng covered by one tile.
  // Note that this is the logical value (1 degree, 0.25 degree), not necessarily
  // the precise value covered, including border samples.
  virtual double degreesAcross() const;

  // Does this format use UTM coordinates rather than lat/lng?
  bool isUtm() const;

  // Return a new CoordinateSystem describing the section of the Earth that
  // the given tile with the given origin (lower-left corner) covers.
  CoordinateSystem *coordinateSystemForOrigin(double lat, double lng, int utmZone = 0) const;
  
  // Return a FileFormat object for the given human-readable string,
  // or nullptr if none.
  static FileFormat *fromName(const std::string &name);
  
private:
  Value mValue;
};

// A file format where the size of the tile in degrees and samples
// can be specified.  There must be an integer number of tiles per
// degree.  The data is assumed to be in FLT format.
class CustomFileFormat : public FileFormat {
public:
  CustomFileFormat(double degreesAcross, int samplesAcross) :
      FileFormat(Value::CUSTOM),
      mDegreesAcross(degreesAcross),
      mSamplesAcross(samplesAcross) {
  }
  virtual ~CustomFileFormat() {}

  virtual int rawSamplesAcross() const {
    return mSamplesAcross;
  }

  virtual int inMemorySamplesAcross() const {
    return mSamplesAcross;
  }
  
  virtual double degreesAcross() const {
    return mDegreesAcross;
  }

private:
  double mDegreesAcross;
  int mSamplesAcross;
};

#endif  // _FILE_FORMAT_H_
