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

#include "prominence_task.h"
#include "coordinate_system.h"
#include "divide_tree.h"
#include "island_tree.h"
#include "tree_builder.h"

#include "easylogging++.h"

#include <limits.h>
#include <stdio.h>
#include <cmath>
#include <memory>
#include <set>

using std::set;
using std::string;
using std::vector;

ProminenceTask::ProminenceTask(TileCache *cache, const ProminenceOptions &options) {
  mCache = cache;
  mOptions = options;
}

bool ProminenceTask::run(float lat, float lng, const CoordinateSystem &coordinateSystem) {
  mCurrentLatitude = lat;
  mCurrentLongitude = lng;
  
  // Load the main tile manually; cache could delete it if we allow it to be cached
  std::unique_ptr<Tile> tile(mCache->loadWithoutCaching(lat, lng, coordinateSystem));
  if (tile.get() == nullptr) {
    VLOG(2) << "Couldn't load tile for " << lat << " " << lng;
    return false;
  }

  // Flip tile upside down if we're computing anti-prominence
  if (mOptions.antiprominence) {
    tile->flipElevations();
  }

  // Build divide tree
  TreeBuilder *builder = new TreeBuilder(tile.get(), coordinateSystem);
  DivideTree *divideTree = builder->buildDivideTree();
  delete builder;

  //
  // Write full divide tree
  //

  if (mOptions.writeFullDivideTree) {
    if (!divideTree->writeToFile(getFilenamePrefix() + "-divide_tree.dvt")) {
      LOG(ERROR) << "Failed to save divide tree file";
    }
    writeStringToOutputFile("divide_tree.kml", divideTree->getAsKml());
  }
  
  //
  // Prune low-prominence peaks to reduce divide tree size.  Rebuild island tree.
  //
  VLOG(1) << "Pruning divide tree to " << mOptions.minProminence << " prominence";

  IslandTree islandTree(*divideTree);
  islandTree.build();

  divideTree->prune(mOptions.minProminence, islandTree);

  //
  // Write pruned divide tree
  //

  string pruned_name("divide_tree_pruned_" + std::to_string(int(mOptions.minProminence)));
  if (!divideTree->writeToFile(getFilenamePrefix() + "-" + pruned_name + ".dvt")) {
    LOG(ERROR) << "Failed to save pruned divide tree file";
  }
  writeStringToOutputFile(pruned_name + ".kml", divideTree->getAsKml());

  delete divideTree;

  return true;
}

bool ProminenceTask::writeStringToOutputFile(const string &filename, const string &str) const {
  string fullFilename = getFilenamePrefix() + "-" + filename;
  FILE *file = fopen(fullFilename.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  fprintf(file, "%s", str.c_str());
  fclose(file);
  return true;
}

string ProminenceTask::getFilenamePrefix() const {
  char filename[PATH_MAX];
  int latHundredths = fractionalDegree(mCurrentLatitude);
  int lngHundredths = fractionalDegree(mCurrentLongitude);
  snprintf(filename, sizeof(filename), "prominence-%02dx%02d-%03dx%02d",
           static_cast<int>(mCurrentLatitude), latHundredths,
           static_cast<int>(mCurrentLongitude), lngHundredths);
  return mOptions.outputDir + "/" + filename;
}

int ProminenceTask::fractionalDegree(float degree) const {
  float excess = fabs(degree - static_cast<int>(degree));
  return static_cast<int>(100 * excess);
}
