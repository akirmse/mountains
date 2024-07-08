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

#include "fabdem_loader.h"
#include "tile.h"
#include "util.h"

#include "easylogging++.h"

#include <math.h>
#include <stdio.h>
#include <string>

using std::string;

static const float FABDEM_NODATA_ELEVATION = -9999.0f;

Tile *FabdemLoader::loadTile(const std::string &directory, double minLat, double minLng) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%c%02d%c%03d_FABDEM_V1-0.flt",
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

  const int inputWidth = 3600;
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

  // Overwrites samples array in-place
  for (int i = 0; i < inputHeight; ++i) {
    for (int j = 0; j < inputWidth; ++j) {
      int index = i * inputWidth + j;
      float sample = inbuf[index];

      // Some source data appears to be corrupt
      if (isnan(sample)) {
        VLOG(1) << "Got NaN pixel at " << i << " " << j;
        sample = FABDEM_NODATA_ELEVATION;
      }
      
      // Convert Copernicus NODATA to our NODATA
      if (fabs(sample - FABDEM_NODATA_ELEVATION) < 0.01) {
        inbuf[index] = Tile::NODATA_ELEVATION;
      } else {
        inbuf[index] = sample;
      }
    }
  }
  
  Tile *tile = new Tile(inputWidth, inputHeight, inbuf);

  return tile;  
}
