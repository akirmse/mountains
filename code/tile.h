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

/*
 * A rectangular grid of terrain samples, equally spaced in lat/lng space.
 */

#ifndef _TILE_H_
#define _TILE_H_

#include "primitives.h"
#include "latlng.h"

#include <vector>
#include <string>

enum FileFormat {
  HGT,  // SRTM (90m, 3 arcsecond)
  NED19,     // FLT file containing NED 1/9 arcsecond data
  NED13_ZIP, // ZIP file containing FLT NED 1/3 arcsecond data
  NED1_ZIP,  // ZIP file containing FLT NED 1 arcsecond data
  GLO30,  // Copernicus GLO-30 30m data
};

class Tile {
public:

  // Tile takes ownership of samples, which will be deallocated later via free().
  Tile(int width, int height, Elevation *samples,
       float minLat, float minLng, float maxLat, float maxLng);
  ~Tile();
  
  int width() const { return mWidth; }
  int height() const { return mHeight; }

  // Coordinates of corners; note that samples are edge-centered and extend
  // half a pixel outside this.  Use isInExtents to know if coordinates are
  // inside tile's extents.
  float minLatitude() const { return mMinLat; }
  float maxLatitude() const { return mMaxLat; }
  float minLongitude() const { return mMinLng; }
  float maxLongitude() const { return mMaxLng; }

  bool isInExtents(int x, int y) const {
    return (x >= 0) && (x < mWidth) && (y >= 0) && (y < mHeight);
  }
  bool isInExtents(Offsets offsets) const {
    return isInExtents(offsets.x(), offsets.y());
  }
  bool isInExtents(float latitude, float longitude) const;
  
  Elevation get(Offsets offsets) const {
    return get(offsets.x(), offsets.y());
  }
  
  Elevation get(int x, int y) const {
    return mSamples[y * mWidth + x];
  }

  void set(int x, int y, Elevation elevation) {
    mSamples[y * mWidth + x] = elevation;
  }
  
  // latitude and longitude must be in our extents
  void setLatLng(float latitude, float longitude, Elevation elevation);
  
  Elevation maxElevation() const {
    return mMaxElevation;
  }

  // After setting some values, need to recompute max, which is cached
  void recomputeMaxElevation();
  
  // Return LatLng for given offset into tile
  LatLng latlng(Offsets pos) const;

  // Return pixel offsets into tile matching given coordinates
  Offsets toOffsets(float latitude, float longitude) const;
  
  // Return a scale factor in [0, 1] that should be multipled by any
  // x distance at the given row, to account for the tile samples being
  // progressively squished in height as latitude increases.
  float distanceScaleForRow(int y) const {
    return mLngDistanceScale[y];
  }

  // Return the number of samples in the vertical (latitude) direction
  // guaranteed to cover the given distance in meters
  int numVerticalSamplesForDistance(float distance) const;

  // Flip elevations so that depressions and mountains are swapped.
  // No-data values are left unchanged.
  void flipElevations();

  // Missing data in source
  static const Elevation NODATA_ELEVATION = -32768;
  
private:
  Tile();
  
  int mWidth;
  int mHeight;

  float mMinLat;
  float mMinLng;
  float mMaxLat;
  float mMaxLng;

  // An array with one entry per row of the tile.
  // Each entry is a scale factor in [0, 1] that should be multiplied by
  // any distance in the longitude (x) direction.  This compensates for
  // lines of longitude getting closer as latitude increases.  The value
  // of the factor is the cosine of the latitude of the row.
  float *mLngDistanceScale;

  Elevation mMaxElevation;
  
  Elevation *mSamples;

  // Precompute some internal values after tile is loaded with samples
  void precomputeTileAfterLoad();
  
  Elevation computeMaxElevation() const;
};


#endif  // _TILE_H_
