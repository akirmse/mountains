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

#include "glo_loader.h"
#include "tile.h"
#include "util.h"

#include "easylogging++.h"

#include <math.h>
#include <stdio.h>
#include <string>

using std::string;

static const float COPERNICUS_NODATA_ELEVATION = -32767.0f;
// For ALOS 30 tiles impersonating GLO 30 tiles where GLO 30 doesn't have coverage
static const float ALOSWORLD3D_NODATA_ELEVATION = -9999.0f;

Tile *GloLoader::loadTile(const std::string &directory, double minLat, double minLng) {
  char buf[100];
  snprintf(buf, sizeof(buf), "Copernicus_DSM_COG_10_%c%02d_00_%c%03d_00_DEM.flt",
           (minLat >= 0) ? 'N' : 'S',
           abs(static_cast<int>(minLat)),
           (minLng >= 0) ? 'E' : 'W',
           abs(static_cast<int>(minLng)));
  string filename(buf);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }

  FILE *infile = fopen(filename.c_str(), "rb");
  if (infile == nullptr) {
    VLOG(3) << "Failed to open file " << filename;
    return nullptr;
  }

  const int inputWidth = getWidthForLatitude(minLat);
  const int inputHeight = 3600;
  int num_raw_samples = inputWidth * inputHeight;
  
  float *inbuf = new float[num_raw_samples];
  
  int num_read = static_cast<int>(fread(inbuf, sizeof(float), num_raw_samples, infile));
  fclose(infile);
  if (num_read != num_raw_samples) {
    fprintf(stderr, "Couldn't read tile file: %s, got %d samples expecting %d\n",
            filename.c_str(), num_read, num_raw_samples);
    delete [] inbuf;
    return nullptr;
  }

  // At high latitudes, tiles are shrunk horizontally.  In order to be able to span
  // "seams" between latitudes where the resolution changes, we always expand the
  // tile to "full" resolution.
  const int outputWidth = 3600;
  const int outputHeight = 3600;
  float expansionRatio = static_cast<float>(inputHeight) / inputWidth;
  if (expansionRatio > 1) {
    float *square = new float[outputWidth * outputHeight];
    // Special case for expansionRatio == 1.5: we convert 2 input pixels
    // to 3 output pixels.  AB -> AAB.
    if (inputWidth * 3 / 2 == outputWidth) {
      for (int row = 0; row < inputHeight; ++row) {
        int pos = row * outputWidth;
        for (int col = 0; col < inputWidth; ) {
          float value = inbuf[row * inputWidth + col];
          square[pos++] = value;
          square[pos++] = value;
          ++col;
          value = inbuf[row * inputWidth + col];
          square[pos++] = value;
          ++col;
        }
      }
    } else {
      // Copy input pixel to N consecutive horizontal output pixels
      int ratio = static_cast<int>(expansionRatio);
      for (int row = 0; row < inputHeight; ++row) {
        int pos = row * outputWidth;
        for (int col = 0; col < inputWidth; ++col) {
          float value = inbuf[row * inputWidth + col];
          for (int i = 0; i < ratio; ++i) {
            square[pos++] = value;
          }
        }
      }
    }
    // Use the expanded tile from now on
    delete [] inbuf;
    inbuf = square;
  }

  Elevation *samples = (Elevation *) malloc(sizeof(Elevation) * outputWidth * outputHeight);
  // Discard extra overlap so that just 1 pixel remains around the outsides.
  // Overwrites samples array in-place
  for (int i = 0; i < outputHeight; ++i) {
    for (int j = 0; j < outputWidth; ++j) {
      int index = i * outputWidth + j;
      float sample = inbuf[index];

      // Convert Copernicus NODATA to our NODATA
      if (fabs(sample - COPERNICUS_NODATA_ELEVATION) < 0.01 ||
          fabs(sample - ALOSWORLD3D_NODATA_ELEVATION) < 0.01) {
        samples[index] = Tile::NODATA_ELEVATION;
      } else {
        samples[index] = sample;
      }
    }
  }
  
  Tile *tile = new Tile(outputWidth, outputHeight, samples);

  delete [] inbuf;
  return tile;  
}

int GloLoader::getWidthForLatitude(double minLat) const {
  // See table at
  // https://copernicus-dem-30m.s3.amazonaws.com/readme.html
  if (minLat >= 85 || minLat < -85) {
    return 360;
  }
  if (minLat >= 80 || minLat < -80) {
    return 720;
  }
  if (minLat >= 70 || minLat < -70) {
    return 1200;
  }
  if (minLat >= 60 || minLat < -60) {
    return 1800;
  }
  if (minLat >= 50 || minLat < -50) {
    return 2400;
  }
  return 3600;
}
