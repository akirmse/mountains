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

#include "hgt_loader.h"
#include "tile.h"
#include "util.h"
#include "easylogging++.h"

#include <stdio.h>
#include <string>

using std::string;

static const int16 HGT_NODATA_ELEVATION = -32768;

static uint16 swapByteOrder16(uint16 us) {
  return (us >> 8) | (us << 8);
}

HgtLoader::HgtLoader(const FileFormat &fileFormat) {
  switch (fileFormat.value()) {
  case FileFormat::Value::HGT:
    mTileSize = 1201;
    break;

  case FileFormat::Value::HGT30:
    mTileSize = 3601;
    break;

  default:
    LOG(ERROR) << "Tried to load HGT file from unknown file format " << static_cast<int>(fileFormat.value());
    break;
  }
}

Tile *HgtLoader::loadTile(const std::string &directory, double minLat, double minLng) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%c%02d%c%03d.hgt",
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
  
  int num_samples = mTileSize * mTileSize;
  
  int16 *inbuf = (int16 *) malloc(sizeof(int16) * num_samples);
  
  Tile *retval = nullptr;
  
  int samples_read = static_cast<int>(fread(inbuf, sizeof(int16), num_samples, infile));
  if (samples_read != num_samples) {
    fprintf(stderr, "Couldn't read tile file: %s, got %d samples expecting %d\n",
            filename.c_str(), samples_read, num_samples);
    free(inbuf);
  } else {
    Elevation *samples = (Elevation *) malloc(sizeof(Elevation) * num_samples);
    // SRTM data is in big-endian order; convert to Elevation
    for (int i = 0; i < num_samples; ++i) {
      int16 elevation = swapByteOrder16(inbuf[i]);
      if (elevation == HGT_NODATA_ELEVATION) {
        samples[i] = Tile::NODATA_ELEVATION;
      } else {
        samples[i] = static_cast<Elevation>(elevation);
      }
    }

    retval = new Tile(mTileSize, mTileSize, samples);
  }

  free(inbuf);
  fclose(infile);

  return retval;
}
