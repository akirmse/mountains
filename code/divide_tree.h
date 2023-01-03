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

#ifndef _DIVIDE_TREE_H_
#define _DIVIDE_TREE_H_

#include "coordinate_system.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

class IslandTree;

// Edges in the divide tree connect peaks that have a saddle between
// them, where a walk up the two divides leaving the saddle reach the
// two peaks.
//
// Although the tree is in principle not directed, it's stored as a
// directed graph for ease of implementation.  Each peak has one
// "parent", and the ID of the saddle between the peak and the parent
// peak is stored with the child.  When convenient, the "parent" edges
// may be reversed while building up the tree.

class DivideTree {
public:
  struct Node {
    Node() : parentId(Null), saddleId(Null) {
    }
      
    int parentId;  // ID of parent node, Null if none
    int saddleId;  // ID of saddle between this peak and its parent, Null if none
    
    static const int Null = -1;
  };

  DivideTree(const CoordinateSystem &coords,
             const std::vector<Peak> &peaks, const std::vector<Saddle> &saddles,
             const std::vector<Runoff> &runoffs);
  
  // Attempt to add an edge between peak1 and peak2, going through the
  // given saddle.
  //
  // If the edge would create a cycle, the edge in the cycle with
  // the lowest saddle is removed (potentially the given edge).
  //
  // Return the index of saddle that was removed (a basin saddle), or Node::Null if none.
  int maybeAddEdge(int peakId1, int peakId2, int saddleId);

  // Add an edge between the given peak and runoff.
  void addRunoffEdge(int peakId, int runoffId);

  // Prune tree of all peaks < the given prominence value.  This is
  // best-effort, not a guarantee.  Some peaks with lower prominence
  // may remain, especially around the edges.
  //
  // The given islandTree must contain up-to-date prominence values.
  // islandTree becomes invalid upon return, since the divide tree has been modified.
  void prune(Elevation minProminence, const IslandTree &islandTree);

  // Merge otherTree into this tree, splicing any matching runoffs.  The two trees
  // must already be in the same coordinate system (i.e. all location values are
  // consistent with each other).
  void merge(const DivideTree &otherTree);
  
  // Change the geographic origin of the tree
  bool setOrigin(const CoordinateSystem &coordinateSystem);

  // Delete any false saddles.  This is an optimization to save space.
  void compact();

  // Delete all runoffs, presumably as a way to clean up a divide tree that's never going
  // to be merged with another one.
  void deleteRunoffs();

  // Flip elevations so that depressions and mountains are swapped.
  void flipElevations();

  bool writeToFile(const std::string &filename) const;

  static DivideTree *readFromFile(const std::string &filename);
  
  void debugPrint() const;

  std::string getAsKml() const;

  const CoordinateSystem &coordinateSystem() const;
  const std::vector<Peak> &peaks() const;
  const std::vector<Saddle> &saddles() const;
  const std::vector<Runoff> &runoffs() const;
  const std::vector<int> &runoffEdges() const;
  const std::vector<Node> &nodes() const;

  void setSaddles(const std::vector<Saddle> saddles);
  
private:

  // Make the given node the root of its tree by reversing parent links.
  void makeNodeIntoRoot(int nodeId);

  // Return the ID of the peak with the lowest saddle between the
  // child and ancestor peak IDs, or Node::Null if the traversal reaches
  // the root of the tree before reaching the ancestor (an error),
  // or if the child and ancestor are the same.
  int findLowestSaddleOnPath(int childPeakId, int ancestorPeakId);

  // Return the lowest node that is the common ancestor of the two given nodes,
  // or Node::Null if there isn't one.
  int findCommonAncestor(int nodeId1, int nodeId2);

  // Return the depth of the given node in the tree.  A node with no parent has depth 1.
  int getDepth(int nodeId);

  // Convert pairs of runoffs at the same location into saddles
  void spliceAllRunoffs();
  
  // Splice the two given runoffs together.  removedRunoffs is updated to contain the indices
  // of any runoffs that are deleted.
  void spliceTwoRunoffs(int index1, int index2, std::unordered_set<int> *removedRunoffs);

  // Remove the given peak.  neighborPeakId gives the neighboring peak whose connecting saddle
  // edge in the tree should be removed.  If there's no saddle between them, then peakId's
  // highest neighboring saddle will be removed.
  void removePeak(int peakId, int neighborPeakId);
  
  std::string getKmlForSaddle(const Saddle &saddle, const char *styleUrl, int index) const;

  // Given a set of indices to delete, build up a vector that tells, for each individual
  // index, how many deleted indices are <= the given index.  So if deletedIndices is
  // { 3, 5, 6 }, deletionOffsets might end up [0, 0, 0, 1, 1, 2, 3, 3, 3].
  // deletionOffsets must be initialized to a vector of 0s of the desired length.
  void computeDeletionOffsets(const std::unordered_set<int> &deletedIndices,
                              std::vector<int> &deletionOffsets) const;

  // Given sets of peak and saddle indices to delete, actually remove them from our
  // internal collections, updating any pointers in nodes to account for the
  // renumbering of indices due to deletions.
  void removeDeletedPeaksAndSaddles(const std::unordered_set<int> &deletedPeakIndices,
                                    const std::unordered_set<int> &deletedSaddleIndices);
  
  // Indices start at 1; use these helper functions to deal with offset.
  const Peak &getPeak(int peakId) const;
  const Saddle &getSaddle(int saddleId) const;

  // Convert pixel offsets to LatLng
  LatLng getLatLng(Offsets offsets) const;

  std::unique_ptr<CoordinateSystem> mCoordinateSystem;
  std::vector<Peak> mPeaks;
  std::vector<Saddle> mSaddles;
  std::vector<Runoff> mRunoffs;

  std::vector<Node> mNodes;
  // Holds peak ID connected to each runoff (parallel array to mRunoffs)
  std::vector<int> mRunoffEdges;

  // Holds peak and saddle IDs to delete temporarily during a merge step.
  std::unordered_set<int> mRemovedPeakIndices;
  std::unordered_set<int> mRemovedSaddleIndices;
};

#endif  // _DIVIDE_TREE_H_
