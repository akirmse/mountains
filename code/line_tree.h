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

#ifndef _LINE_TREE_H_
#define _LINE_TREE_H_

#include "primitives.h"
#include "divide_tree.h"

#include <vector>

// In the line tree, the parent of a peak is the first higher peak
// encountered walking the divide tree, staying as high as possible.
//
// As a side effect, building the line tree computes the "prominence"
// for each saddle.  The prominence of a saddle is the prominence of
// the peak for which it is the key saddle.  When pruning a tree of
// all peaks under a prominence P, not only must all peaks with
// prominence >= P be kept, but so must their key saddles, i.e. all
// saddles with prominence >= P.

class LineTree {
public:
  struct Node {
    int parentId;
    int saddleId;  // Saddle between us and line tree parent (not divide tree parent)
    Elevation lowestElevationSaddleChildDir;  // lowest saddle in direction of children
    Elevation lowestElevationSaddleParentDir;  // lowest saddle in direction of parents
    int childId;  // ID of node whose parent we are
    int runoffId;  // Index of runoff pointing at this peak

    static const int Null = -1;
  };

  explicit LineTree(const DivideTree &divideTree);

  void build();

  // Return true if the saddle has at least the given minimum
  // prominence.
  bool saddleHasMinProminence(int saddleId, Elevation minProminence);

  bool writeToFile(const std::string &filename) const;

  const std::vector<Node> &nodes() const;

private:
  struct SaddleInfo {
    // Upper bound on saddle's prominence (including the possibility
    // of going off the map through a runoff).
    Elevation saddleProminence;
  };
  
  const DivideTree &mDivideTree;

  std::vector<Node> mNodes;
  std::vector<SaddleInfo> mSaddleInfo;

  // The lowest saddle on any runoff->runoff path through the divide
  // tree has potentially unlimited prominence, since there could be a
  // peak off the map for which the low saddle is the key saddle.
  // Thus, this saddle cannot be pruned before more of the map is
  // seen.
  //
  // This function finds those saddles and sets their prominence to
  // effectively infinite.
  void computeOffMapSaddleProminence();

  // For every peak whose key saddle is on the map, compute the prominence
  // of the saddle.
  void computeOnMapSaddleProminence();
  
  // Reverse the parent pointers, originally going from startPeakId to endPeakId.
  void reversePath(int startPeakId, int endPeakId);

  // Propagate the given node's lowestSaddleElevationParentDir up the
  // chain of childId until we reach a lower saddle.
  void propagateLowestInterveningSaddle(int originNodeId);

  // For debugging: Look for cycles in the tree and print them out
  bool hasCycle(int maxLength) const;

  const Peak &getPeak(int peakId) const;
  const Saddle &getSaddle(int saddleId) const;
  const Runoff &getRunoff(int runoffId) const;
  int peakIdForRunoff(int runoffId) const;
  const DivideTree::Node &getDivideTreeNode(int nodeId) const;
  const Saddle &getSaddleForPeakId(int peakId) const;
};

#endif  // _LINE_TREE_H_
