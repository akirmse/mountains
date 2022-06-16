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

#include "flt_loader.h"

#include <math.h>
#include <stdio.h>

using std::string;

// NED FLT files have an overlap of 6 pixels on all sizes.  We want just 1.  To get that,
// we remove all overlap pixels on the top and left, and leave one on the bottom and right.
// See https://ca.water.usgs.gov/projects/sandiego/data/gis/dem/ned13/readme.pdf
static const int FLT_EXTRA_BORDER = 6;
static const int FLT_13_RAW_SIZE = 10812;
static const int FLT_1_RAW_SIZE = 3612;

static const float NED_NODATA_ELEVATION = -9999;

FltLoader::FltLoader(FileFormat format) {
  mFormat = format;
}

Tile *FltLoader::loadTile(const std::string &directory, int minLat, int minLng) {
  return loadFromNEDZipFileInternal(directory, minLat, minLng, mFormat);
}

Tile *FltLoader::loadFromFltFile(const string &directory, int minLat, int minLng, FileFormat format) {
  string filename = getFltFilename(minLat, minLng, format);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }
  
  FILE *infile = fopen(filename.c_str(), "rb");
  if (infile == nullptr) {
    VLOG(3) << "Failed to open file " << filename;
    return nullptr;
  }

  const int rawSideLength = (format == FileFormat::NED13_ZIP ? FLT_13_RAW_SIZE : FLT_1_RAW_SIZE);
  const int tileSideLength = rawSideLength - 2 * FLT_EXTRA_BORDER + 1;
  int num_raw_samples = rawSideLength * rawSideLength;
  
  Elevation *samples = (Elevation *) malloc(sizeof(Elevation) * tileSideLength * tileSideLength);
  float *inbuf = new float[num_raw_samples];
  
  Tile *retval = nullptr;
  int num_read = static_cast<int>(fread(inbuf, sizeof(float), num_raw_samples, infile));
  if (num_read != num_raw_samples) {
    fprintf(stderr, "Couldn't read tile file: %s, got %d samples expecting %d\n",
            filename.c_str(), num_read, num_raw_samples);
    free(samples);
    samples = nullptr;
  } else {
    // Discard extra overlap so that just 1 pixel remains around the outsides.
    // Overwrites samples array in-place
    for (int i = 0; i < rawSideLength; ++i) {
      for (int j = 0; j < rawSideLength; ++j) {
        float sample = inbuf[i * rawSideLength + j];
        
        // Convert NED nodata to SRTM nodata
        if (i >= FLT_EXTRA_BORDER && i < tileSideLength + FLT_EXTRA_BORDER &&
            j >= FLT_EXTRA_BORDER && j < tileSideLength + FLT_EXTRA_BORDER) {
          int index = ((i - FLT_EXTRA_BORDER) * tileSideLength) + (j - FLT_EXTRA_BORDER);
          if (fabs(sample - NED_NODATA_ELEVATION) < 0.01) {
            samples[index] = Tile::NODATA_ELEVATION;
          } else {
            // Use feet internally; small unit avoids losing precision with external data
            samples[index] = (Elevation) metersToFeet(sample);
          }
        }
      }
    }
  }
  
  if (samples != nullptr) {
    // Tile is 1 square degree
    float fMinLat = static_cast<float>(minLat);
    float fMinLng = static_cast<float>(minLng);
    retval = new Tile(tileSideLength, tileSideLength, samples,
                      fMinLat, fMinLng, fMinLat + 1, fMinLng + 1);
  }

  delete [] inbuf;
  fclose(infile);

  return retval;  
}

Tile *FltLoader::loadFromNEDZipFileInternal(const std::string &directory,
                                            int minLat, int minLng, FileFormat format) {
  char buf[100];
  sprintf(buf, "%c%02d%c%03d.zip",
          (minLat >= 0) ? 'n' : 's',
          abs(minLat + 1),  // NED uses upper left corner for naming
          (minLng >= 0) ? 'e' : 'w',
          abs(minLng));
  string filename(buf);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }

  if (!fileExists(filename)) {
    VLOG(1) << "Input tile " << filename << " doesn't exist; skipping";
    return nullptr;
  }
  
  string tempDirectory = getTempDir();
  string fltFilename = getFltFilename(minLat, minLng, format);

  // Unzip flt file from zip to temp dir
#ifdef PLATFORM_WINDOWS
  string command = "7z x \"" + filename + "\" " + fltFilename + " -y -o" + tempDirectory
    + " > nul";
#else  
  string command = "unzip -o \"" + filename + "\" " + fltFilename + " -d " + tempDirectory;
#endif
  VLOG(2) << "Unzip command is " << command;
  int retval = system(command.c_str());
  if (retval != 0) {
    LOG(ERROR) << "Command failed: " << command;
  }
  
  Tile *tile = loadFromFltFile(tempDirectory, minLat, minLng, format);

  // Delete temp file
  string fltFullFilename = tempDirectory + "/" + fltFilename;
  remove(fltFullFilename.c_str());
  
  return tile;  
}

string FltLoader::getFltFilename(int minLat, int minLng, FileFormat format) {
  char buf[100];
  sprintf(buf, "float%c%02d%c%03d_%s.flt",
          (minLat >= 0) ? 'n' : 's',
          abs(minLat + 1),  // NED uses upper left corner for naming
          (minLng >= 0) ? 'e' : 'w',
          abs(minLng),
          format == FileFormat::NED13_ZIP ? "13" : "1");
  return buf;
}
