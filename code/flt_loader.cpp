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

// Anything less than this value is considered NODATA.  NED1 and NED13 use -9999,
// while NED19 uses some very negative value (< -1e38).
static const float NED_NODATA_MIN_ELEVATION = -9998;

FltLoader::FltLoader(const FileFormat &format, int utmZone) :
    mFormat(format),
    mUtmZone(utmZone) {
}

Tile *FltLoader::loadTile(const std::string &directory, double minLat, double minLng) {
  switch (mFormat.value()) {
  case FileFormat::Value::NED13_ZIP:  // fall through
  case FileFormat::Value::NED1_ZIP:
    return loadFromNEDZipFileInternal(directory, minLat, minLng);

  case FileFormat::Value::NED13:
  case FileFormat::Value::NED19:
  case FileFormat::Value::THREEDEP_1M:
  case FileFormat::Value::CUSTOM:
    return loadFromFltFile(directory, minLat, minLng);

  default:
    LOG(ERROR) << "Got unknown tile file format in FltLoader";
    return nullptr;
  }
}

Tile *FltLoader::loadFromFltFile(const string &directory, double minLat, double minLng) {
  string filename = getFltFilename(minLat, minLng, mFormat);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }

  FILE *infile = fopen(filename.c_str(), "rb");
  if (infile == nullptr) {
    VLOG(3) << "Failed to open file " << filename;
    return nullptr;
  }

  const int rawSideLength = mFormat.rawSamplesAcross();
  const int tileSideLength = mFormat.inMemorySamplesAcross();
  // NED FLT files have an overlap of 6 pixels on all sizes.  We want just 1.  To get that,
  // we remove all overlap pixels on the top and left, and leave one on the bottom and right.
  // See https://ca.water.usgs.gov/projects/sandiego/data/gis/dem/ned13/readme.pdf
  const int extraBorder = std::max(0, (rawSideLength - tileSideLength + 1) / 2);
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
        if (i >= extraBorder && i < tileSideLength + extraBorder &&
            j >= extraBorder && j < tileSideLength + extraBorder) {
          int index = ((i - extraBorder) * tileSideLength) + (j - extraBorder);
          if (sample < NED_NODATA_MIN_ELEVATION) {
            samples[index] = Tile::NODATA_ELEVATION;
          } else {
            samples[index] = sample;
          }
        }
      }
    }
  }
  delete [] inbuf;
  fclose(infile);

  if (samples != nullptr) {
    retval = new Tile(tileSideLength, tileSideLength, samples);
  }

  return retval;  
}

Tile *FltLoader::loadFromNEDZipFileInternal(const std::string &directory,
                                            double minLat, double minLng) {
  // ZIP formats come only in 1x1 degree formats, so OK to cast lat/lng to int.
  // NED uses upper left corner for naming.
  char buf[100];
  snprintf(buf, sizeof(buf), "%c%02d%c%03d.zip",
           (minLat >= 0) ? 'n' : 's',
           abs(static_cast<int>(minLat + mFormat.degreesAcross())),
           (minLng >= 0) ? 'e' : 'w',
           abs(static_cast<int>(minLng)));
  string filename(buf);
  if (!directory.empty()) {
    filename = directory + "/" + filename;
  }

  if (!fileExists(filename)) {
    VLOG(1) << "Input tile " << filename << " doesn't exist; skipping";
    return nullptr;
  }
  
  string tempDirectory = getTempDir();
  string fltFilename = getFltFilename(minLat, minLng, mFormat);

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
  
  Tile *tile = loadFromFltFile(tempDirectory, minLat, minLng);

  // Delete temp file
  string fltFullFilename = tempDirectory + "/" + fltFilename;
  remove(fltFullFilename.c_str());
  
  return tile;  
}

string FltLoader::getFltFilename(double minLat, double minLng, const FileFormat &format) {
  char buf[100];
  // NED uses upper left corner for naming
  auto upperLat = minLat + format.degreesAcross();
  switch (format.value()) {
  case FileFormat::Value::NED13_ZIP:  // fall through
  case FileFormat::Value::NED1_ZIP:
    snprintf(buf, sizeof(buf), "float%c%02d%c%03d_%s.flt",
             (minLat >= 0) ? 'n' : 's',
             abs(static_cast<int>(upperLat)),
             (minLng >= 0) ? 'e' : 'w',
             abs(static_cast<int>(minLng)),
             format.value() == FileFormat::Value::NED13_ZIP ? "13" : "1");
    break;

  case FileFormat::Value::NED13:
    snprintf(buf, sizeof(buf), "USGS_13_%c%02d%c%03d.flt",
             (minLat >= 0) ? 'n' : 's',
             abs(static_cast<int>(upperLat)),
             (minLng >= 0) ? 'e' : 'w',
             abs(static_cast<int>(minLng)));
    break;
    
  case FileFormat::Value::NED19:
    snprintf(buf, sizeof(buf), "ned19_%c%02dx%02d_%c%03dx%02d.flt",
             (minLat >= 0) ? 'n' : 's',
             abs(static_cast<int>(upperLat)),
             fractionalDegree(upperLat),
             (minLng >= 0) ? 'e' : 'w',
             abs(static_cast<int>(minLng)),
             fractionalDegree(minLng));
    break;

  case FileFormat::Value::CUSTOM:
    snprintf(buf, sizeof(buf), "tile_%02dx%02d_%03dx%02d.flt",
             static_cast<int>(upperLat),
             fractionalDegree(upperLat),
             static_cast<int>(minLng),
             fractionalDegree(minLng));
    break;

  case FileFormat::Value::THREEDEP_1M:
    // Note order: X (lng), then Y (lat)
    snprintf(buf, sizeof(buf), "USGS_1M_%02d_x%02dy%03d.flt",
             mUtmZone, static_cast<int>(minLng), static_cast<int>(minLat));
    break;
    
  default:
    printf("Couldn't compute filename for unknown FLT file format");
    exit(1);
  }
  
  return buf;
}

int FltLoader::fractionalDegree(double degree) const {
  double excess = fabs(degree - static_cast<int>(degree));
  return static_cast<int>(std::round(100 * excess));
}
