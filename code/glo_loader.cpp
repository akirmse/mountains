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

#include <stdio.h>
#include <string>

// TODO: Resample up to 3600x3600
// TODO: Read right and bottom pixels from neighbor tiles

using std::string;

Tile *GloLoader::loadTile(const std::string &directory, int minLat, int minLng) {
  char buf[100];
  sprintf(buf, "Copernicus_DSM_COG_10_%c%02d_00_%c%03d_00_DEM.flt",
          (minLat >= 0) ? 'N' : 'S',
          abs(minLat),
          (minLng >= 0) ? 'E' : 'W',
          abs(minLng));
  string filename(buf);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }

  FILE *infile = fopen(filename.c_str(), "rb");
  if (infile == nullptr) {
    VLOG(3) << "Failed to open file " << filename;
    return nullptr;
  }

  const int width = getWidthForLatitude(minLat);
  const int height = 3600;
  int num_raw_samples = width * height;
  
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
  float expansionRatio = static_cast<float>(height) / width;
  if (expansionRatio > 1) {
    // XXX 1.5
    float *square = new float[height * height];
    int ratio = static_cast<int>(expansionRatio);
    for (int row = 0; row < height; ++row) {
      int pos = row * height;
      for (int col = 0; col < width; ++col) {
        float value = inbuf[row * width + col];
        for (int i = 0; i < ratio; ++i) {
          square[pos++] = value;
        }
      }
    }
    // Use the expanded tile from now on
    delete [] inbuf;
    inbuf = square;
  }
  
  Elevation *samples = (Elevation *) malloc(sizeof(Elevation) * width * height);
  // Discard extra overlap so that just 1 pixel remains around the outsides.
  // Overwrites samples array in-place
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      int index = i * width + j;
      float sample = inbuf[index];
      
      // XXX NODATA?
      // Use feet internally; small unit avoids losing precision with external data
      samples[index] = (Elevation) metersToFeet(sample);
    }
  }
  
  // Tile is 1 square degree
  float fMinLat = static_cast<float>(minLat);
  float fMinLng = static_cast<float>(minLng);
  Tile *tile = new Tile(width, height, samples,
                        fMinLat, fMinLng, fMinLat + 1, fMinLng + 1);

  delete [] inbuf;
  return tile;  
}

int GloLoader::getWidthForLatitude(int minLat) const {
  // See table at
  // https://copernicus-dem-30m.s3.amazonaws.com/readme.html
  if (minLat >= 85) {
    return 360;
  }
  if (minLat >= 80) {
    return 720;
  }
  if (minLat >= 70) {
    return 1200;
  }
  if (minLat >= 60) {
    return 1800;
  }
  if (minLat >= 50) {
    return 2400;
  }
  return 3600;
}
