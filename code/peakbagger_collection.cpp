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

#include "peakbagger_collection.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <assert.h>
#include <string.h>

using std::string;

static bool StringStartsWith(const std::string &str,
                             const char *prefix) {
   return 0 == strncmp(str.c_str(), prefix, strlen(prefix));
}

PeakbaggerCollection::PeakbaggerCollection() {
}

bool PeakbaggerCollection::Load(const std::string &filename) {
   std::ifstream file(filename.c_str());
   std::string val;
   int line = 0;
   while (file.good()) {
      ++line;
      getline(file, val, '>');

      // Remove trailing whitespace
      val.erase(val.find_last_not_of(" \n\r\t") + 1);
      
      // Skip blank lines and XML start/end
      if (val.empty() || StringStartsWith(val, "<?xml") ||
          StringStartsWith(val, "<ex") ||
          StringStartsWith(val, "</ex")) {
         continue;
      }

      float lat, lng, isolation;
      int id, elevation;
      char name[1024];
      char state[1024];
      char prominence_string[1024];
      char loj_string[1024];
      int num_matched =
         sscanf(val.c_str(),
                "<pk i=\"%d\" n=\"%1000[^\"]\" e=\"%d\" r=\"%1000[^ ] s=\"%f\" a=\"%f\" o=\"%f\" l=\"%100[^\"]\" lj=\"%1000[^ /]/>",
                &id, name, &elevation, (char *) &prominence_string, &isolation, &lat, &lng, state, loj_string);
      if (num_matched != 9) {
         fprintf(stderr, "Format error on line %d (%d matched): %s\n",
                 line, num_matched, val.c_str());
         return false;
      }

      int loj_id = 0;
      // Is the LoJ ID field non-empty?
      if (loj_string[0] != '"') {
         num_matched = sscanf(loj_string, "%d", &loj_id);
         if (num_matched != 1) {
            fprintf(stderr, "Format error with LoJ ID on line %d: %s\n",
                    line, loj_string);
         }
      }

      // Has a prominence value?
      int prominence = atoi(prominence_string);
      if (prominence_string[0] == '\"') {
        prominence = PeakbaggerPoint::MISSING_PROMINENCE;
      }

      // Replace commas with semicolons in peak name (we use comma as separator)
      string peak_name(name);
      std::replace(peak_name.begin(), peak_name.end(), ',', ';');
      
      // Skip non-existent historical peaks
      if (!StringStartsWith(peak_name, "Pre-")) {
        PeakbaggerPoint p(id, name, lat, lng, elevation, prominence, isolation, state, loj_id);
        mPoints.push_back(p);
      }
   }
   
   return true;
}

void PeakbaggerCollection::InsertIntoQuadtree(Quadtree *tree) {
   assert(tree != NULL);

   for (auto it = mPoints.begin(); it != mPoints.end(); ++it) {
      tree->Insert(*it);
   }
}

void PeakbaggerCollection::SortByProminence() {
  std::sort(mPoints.begin(), mPoints.end(), [](const PeakbaggerPoint &p1, const PeakbaggerPoint &p2) {
      return p1.prominence() > p2.prominence();
    });
}

const std::vector<PeakbaggerPoint> &PeakbaggerCollection::points() const {
   return mPoints;
}
