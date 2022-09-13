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

#include "quadtree.h"

#include <algorithm>
#include <set>
#include <assert.h>


//
// The quadtree is a recursive structure that has the following
// layout at each level.
//
// +-----+-----+
// |  0  |  1  |
// +-----+-----+
// |  2  |  3  |
// +-----+-----+
//
// The nodes of the tree are stored in a vector, which is somewhat
// wasteful of space, especially as the max level gets deeper.
// However it makes traversing the tree fast.
//
// At each leaf node is a vector of contents (or NULL if the node is
// empty).  They are not stored in any particular order.
//

using std::set;
using std::vector;

Quadtree::Quadtree(int max_level) {
   assert(max_level >= 0);
   
   mMaxLevel = max_level;
   mCells.resize(1ULL << (2 * max_level));
}

void Quadtree::Insert(const LatLng &p) {
   int index = GetIndexForLatLng(p.latitude(), p.longitude());
   if (mCells[index] == NULL)
      mCells[index] = new vector<const LatLng *>();
   mCells[index]->push_back(&p);
}

void Quadtree::InsertAll(const vector<LatLng> &points) {
   for (const LatLng &point : points) {
      Insert(point);
   }
}

bool Quadtree::Remove(const LatLng *point) {
   int index = GetIndexForLatLng(point->latitude(), point->longitude());
   Cell *cell = mCells[index];
   if (cell != NULL) {
      for (int i = 0; i < (int) cell->size(); ++i) {
         if ((*cell)[i] == point) {
            cell->erase(cell->begin() + i);
            return true;
         }
      }
   }

   return false;
}
   

void Quadtree::Lookup(const LatLng &p, std::vector<const LatLng *> *neighbors,
                      float threshold_meters) const {
   int p_index = GetIndexForLatLng(p.latitude(), p.longitude());
   set<int> indices;
   indices.insert(p_index);

   // Look at neighboring cells by computing bounding box of a spherical
   // cap threshold_meters across, and computing the node indices for
   // the four corners.  Uniquify them, since some (or all) of the
   // corners may be in the same node.
   vector<LatLng> box = p.GetBoundingBoxForCap(threshold_meters);
   indices.insert(GetIndexForLatLng(box[0].latitude(), box[0].longitude()));
   indices.insert(GetIndexForLatLng(box[0].latitude(), box[1].longitude()));
   indices.insert(GetIndexForLatLng(box[1].latitude(), box[0].longitude()));
   indices.insert(GetIndexForLatLng(box[1].latitude(), box[1].longitude()));

   for (auto index : indices) {
      vector<const LatLng *> *points = mCells[index];
      if (points != NULL) {
         for (auto point : *points) {
            if (p.distance(*point) <= threshold_meters) {
               neighbors->push_back(point);
            }
         }
      }
   }
}

int Quadtree::GetIndexForLatLng(double lat, double lng) const {
   int level = 0;
   double left = -180;
   double right = 180;
   double top = 90;
   double bottom = -90;
   int index = 0;
   int bucket = static_cast<int>(mCells.size());
   while (level < mMaxLevel) {
      double mid_lng = (left + right) / 2;
      double mid_lat = (top + bottom) / 2;
      if (lat >= mid_lat) {
         bottom = mid_lat;
      } else {
         index += bucket / 2;
         top = mid_lat;
      }

      if (lng >= mid_lng) {
         left = mid_lng;
         index += bucket / 4;
      } else {
         right = mid_lng;
      }
      
      bucket >>= 2;
      ++level;
   }
   return index;
}
