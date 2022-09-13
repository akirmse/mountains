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


#ifndef _QUADTREE_H__
#define _QUADTREE_H__

#include <vector>

#include "latlng.h"

class Quadtree {
public:
   // Points will be inserted at the given level of the tree.
   // The space used by the tree is quadradic in max_level.
   explicit Quadtree(int max_level);

   // Insert the given point to the quadtree.
   // A copy is not made; the point must live as long as the quadtree.
   void Insert(const LatLng &p);

   // Insert all of the points into the quadtree
   void InsertAll(const std::vector<LatLng> &points);

   // Remove the point with the given ID at the given position.
   // Return true if it was found and removed.
   bool Remove(const LatLng *point);
   
   // Find all points in the tree within threshold meters of the given
   // one, and put them in "neighbors".
   //
   // threshold_meters must be no larger than the smallest edge of a
   // leaf quadtree cell, or else some neighbors may be missed.
   void Lookup(const LatLng &p, std::vector<const LatLng *> *neighbors,
               float threshold_meters) const;
   
private:
   int GetIndexForLatLng(double lat, double lng) const;

   int mMaxLevel;
   typedef std::vector<const LatLng *> Cell;
   std::vector<Cell *> mCells;
};

#endif // ifndef _QUADTREE_H__
