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

#ifndef _TREE_BUILDER_H_
#define _TREE_BUILDER_H_

// This class is responsible for converting a terrain tile into a
// divide tree.  The algorithm is:
//
// * Find all peaks and saddles.  A peak is a flat area higher than
// its entire boundary.  A saddle is a flat area with at least two
// non-touching higher areas in its boundary.  If there are more than
// two higher areas, the saddle must be split into multiple saddles,
// each with a multiplicity of 2.
//
// * Walk up divides (via steepest ascent) from each saddle to two
// peaks.  If the walk encounters a flat region, find the highest
// point adjacent to the flat region.
//
// * If the two walks reach the same peak, the saddle is false and
// is discarded.  Otherwise, an edge is added to the divide tree
// between the two peaks.  If this would create a cycle, the edge with
// the lowest saddle on the cycle is removed.  This lowest saddle
// is called a "basin saddle" and is not used past this point.
//
// * Compute runoffs around the edge of the tile, and add runoff->peak
// edges to the divide tree by walking uphill from each runoff.

#include <memory>
#include <stack>
#include <vector>
#include "primitives.h"
#include "domain_map.h"
#include "tile.h"

class CoordinateSystem;
class DivideTree;

class TreeBuilder {
public:
  TreeBuilder(const Tile *tile, const CoordinateSystem &coordinateSystem);

  DivideTree *buildDivideTree();
  
private:
  std::vector<Peak> mPeaks;
  std::vector<Saddle> mSaddles;  // all saddles
  std::vector<Runoff> mRunoffs;

  struct PerSaddleInfo {
    PerSaddleInfo(Offsets r1, Offsets r2) :
        rise1(r1), rise2(r2) {
    }
    
    // Coordinates of two higher points around saddle's boundary.  First point
    // is the higher of the two.
    Offsets rise1;
    Offsets rise2;
  };

  // Parallel array with mSaddles
  std::vector<PerSaddleInfo> mSaddleInfo;
  
  DomainMap mDomainMap;
  std::stack<Offsets> mPendingStack;  // member var to avoid frequent allocations
  
  const Tile *mTile;
  std::unique_ptr<CoordinateSystem> mCoordinateSystem;

  // Find peaks, saddles, runoffs
  void findExtrema();
  void findRunoffs();
  
  DivideTree *generateDivideTree();

  std::vector<Offsets> walkUpToPeak(Offsets startPoint);
  const Saddle &getSaddle(DomainMap::Pixel domainPixel) const;
  const PerSaddleInfo &getSaddleInfo(DomainMap::Pixel domainPixel) const;

  // Return the highest neighboring point higher than the given
  // point.  If no point is higher, returns the given point.
  Offsets findSteepestNeighbor(Offsets point) const;
};

#endif  // _TREE_BUILDER_H_
