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

#include "prominence_collection.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <assert.h>

ProminenceCollection::ProminenceCollection() {
}

bool ProminenceCollection::Load(const std::string &filename) {
   std::ifstream file(filename.c_str());
   std::string val;
   int line = 0;
   while (file.good()) {
      ++line;
      getline(file, val, '\n');

      // Remove trailing whitespace; skip blank lines
      val.erase(val.find_last_not_of(" \n\r\t") + 1);
      if (val.empty()) {
        continue;
      }

      float lat, lng, saddle_lat, saddle_lng;
      int elevation, prominence;
      int num_matched =
        sscanf(val.c_str(),
               "%f,%f,%d,%f,%f,%d",
               &lat, &lng, &elevation, &saddle_lat, &saddle_lng, &prominence);
      if (num_matched != 6) {
        fprintf(stderr, "Format error on line %d (%d matched): %s\n",
                line, num_matched, val.c_str());
        return false;
      }

      ProminencePoint p(LatLng(lat, lng), elevation, prominence, LatLng(saddle_lat, saddle_lng));
      mPoints.push_back(p);
   }

   return true;
}

void ProminenceCollection::InsertIntoQuadtree(Quadtree *tree) {
   assert(tree != NULL);

   for (auto it = mPoints.begin(); it != mPoints.end(); ++it) {
      tree->Insert(*it);
   }
}

const std::vector<ProminencePoint> &ProminenceCollection::points() const {
   return mPoints;
}
