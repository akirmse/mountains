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


#ifndef _PROMINENCE_TASK_H_
#define _PROMINENCE_TASK_H_

#include "tile_cache.h"

#include <string>

class CoordinateSystem;

struct ProminenceOptions {
  std::string outputDir;
  Elevation minProminence;

  // The full divide tree can be very large, and writing it can be very slow.
  bool writeFullDivideTree;

  // Determine whether this task computes prominence (value=false, the default),
  // or anti-prominence, which is the "prominence" of low points.
  bool antiprominence;

  // Data is bathymetry; do not assume sea level = 0 for prom calculations.
  bool bathymetry;

  // Write KML of pruned divide tree?
  bool writeKml;
};

// Calculate prominence for all peaks in one tile
class ProminenceTask {
public:
  ProminenceTask(TileCache *cache, const ProminenceOptions &options);

  // Returns true if a tile was processed, false if tile couldn't be loaded.
  // lat, lng define the tile to analyze.
  // Output is written to output_dir.
  bool run(double lat, double lng, const CoordinateSystem &coordinateSystem);

  void setAntiprominence(bool value);
  
private:
  TileCache *mCache;
  ProminenceOptions mOptions;

  double mCurrentLatitude;
  double mCurrentLongitude;

  std::string getFilenamePrefix() const;
  // Return hundredths of a degree from the given value
  int fractionalDegree(double degree) const;
  bool writeStringToOutputFile(const std::string &filename, const std::string &str) const;
};

#endif  // _PROMINENCE_TASK_H_
