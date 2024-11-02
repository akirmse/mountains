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

#ifndef _ISLAND_TREE_H_
#define _ISLAND_TREE_H_

#include <string>
#include <vector>

#include "divide_tree.h"

class DivideTree;

// In a prominence island tree, each peak (node) has as its parent a
// higher peak.

class IslandTree {
public:
  struct Node {
    int parentId;  // Higher peak
    int saddlePeakId;  // Peak with highest saddle connected to us
    Elevation prominence;  // UnknownProminence if not known
    int keySaddleId;  // Null if no key saddle
    
    static const int Null = DivideTree::Node::Null;
    static constexpr Elevation UnknownProminence = -32767;
  };

  explicit IslandTree(const DivideTree &divideTree);

  // If isBathymetry is true, don't assume sea level = 0.
  void build(bool isBathymetry);

  bool writeToFile(const std::string &filename) const;

  std::string getAsKml() const;

  const std::vector<Node> &nodes() const;
  
private:
  const DivideTree &mDivideTree;
  std::vector<Node> mNodes;
  
  void uninvertPeaks();

  // Sort peaks by increasing divide tree saddle elevation
  void uninvertSaddles();

  void computeProminences(bool isBathymetry);

  void uninvertPeak(int nodeId);
  void uninvertSaddle(int nodeId);

  // Return the elevation of "sea level", i.e. the elevation used as the
  // base of the highest peak in the tree.
  Elevation getSeaLevelValue(bool isBathymetry);

  // Indices start at 1; use these helper functions to deal with offset.
  const Peak &getPeak(int peakId) const;
  const Saddle &getSaddle(int saddleId) const;

  // Return true if point2 is higher than point1.  The point IDs are used
  // to provide a total ordering (i.e. break ties on elevation).
  bool point2IsHigher(Elevation point1Elevation, int point1Id,
                      Elevation point2Elevation, int point2Id);
};

#endif  // _ISLAND_TREE_H_
