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

#include "line_tree.h"
#include "divide_tree.h"
#include "easylogging++.h"

using std::vector;

static const Elevation HUGE_ELEVATION = 32000;
static const Elevation UNDEFINED_ELEVATION = -10000;

LineTree::LineTree(const DivideTree &divideTree) :
    mDivideTree(divideTree) {
}

void LineTree::build() {
  VLOG(2) << "Building line tree";
  // Initialize topology
  mNodes.resize(mDivideTree.nodes().size());
  for (int index = 1; index < (int) mNodes.size(); ++index) {
    // Copy parent links from divide tree
    mNodes[index].parentId = mDivideTree.nodes()[index].parentId;
    mNodes[index].childId = Node::Null;
    mNodes[index].saddleId = index;
    mNodes[index].lowestElevationSaddleChildDir = UNDEFINED_ELEVATION;
    mNodes[index].lowestElevationSaddleParentDir = UNDEFINED_ELEVATION;
    mNodes[index].runoffId = Node::Null;
  }
  
  mSaddleInfo.resize(mDivideTree.saddles().size());
  for (SaddleInfo &info : mSaddleInfo) {
    info.saddleProminence = UNDEFINED_ELEVATION;
  }

  VLOG(3) << "Computing off-map saddle prominence";
  computeOffMapSaddleProminence();

  VLOG(3) << "Computing on-map saddle prominence";
  computeOnMapSaddleProminence();
}

bool LineTree::writeToFile(const std::string &filename) const {
  FILE *file = fopen(filename.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }

  for (int i = 1; i < (int) mNodes.size(); ++i) {
    const Node &node = mNodes[i];
    Elevation elev = getPeak(i).elevation;
    Elevation parentElev = -1;
    if (node.parentId != Node::Null) {
      parentElev = getPeak(node.parentId).elevation;
    }
    fprintf(file, "%d,%.2f,%d,%.2f\n", i, elev, node.parentId, parentElev);
  }

  fclose(file);
  return true;
}

void LineTree::computeOffMapSaddleProminence() {
  for (int runoffIndex = 0; runoffIndex < (int) mDivideTree.runoffs().size(); ++runoffIndex) {
    const Runoff &runoff = getRunoff(runoffIndex);
    int peakId = peakIdForRunoff(runoffIndex);
    if (peakId == Node::Null) {
      continue;
    }

    VLOG(3) << "Checking runoff " << runoffIndex;
    
    int nodeId = peakId;
    int lowestSaddleOwner = Node::Null;
    Elevation lowestSaddleElevation = runoff.elevation;
    while (true) {
      Node *node = &mNodes[nodeId];

      // Reached top of tree?
      if (node->parentId == Node::Null) {
        break;
      }

      Elevation saddleElevation = getSaddleForPeakId(node->saddleId).elevation;
      if (saddleElevation < lowestSaddleElevation) {
        lowestSaddleOwner = nodeId;
        lowestSaddleElevation = saddleElevation;
      }
      
      nodeId = node->parentId;
    }

    // Is there a runoff pointing at the root?
    if (mNodes[nodeId].runoffId == Node::Null) {
      lowestSaddleOwner = nodeId;
    } else {
      const Runoff &runoff2 = getRunoff(mNodes[nodeId].runoffId);
      if (runoff2.elevation < lowestSaddleElevation) {
        lowestSaddleOwner = nodeId;
        lowestSaddleElevation = runoff2.elevation;
      }

      // The lowest saddles along the path have potentially unlimited
      // prominence, because they could be the key saddle of a huge
      // peak in another tile.
      for (int nid = peakId; nid != nodeId; nid = mNodes[nid].parentId) {
        int saddleOwnerId = mNodes[nid].saddleId;
        Node *saddleOwnerNode = &mNodes[saddleOwnerId];
        Elevation saddleElevation = getSaddleForPeakId(saddleOwnerId).elevation;
        SaddleInfo *saddleInfo = &mSaddleInfo[getDivideTreeNode(saddleOwnerId).saddleId - 1];
        if (saddleElevation <= lowestSaddleElevation &&
            saddleInfo->saddleProminence == UNDEFINED_ELEVATION) {
          saddleInfo->saddleProminence = HUGE_ELEVATION;
        }
      }
    }

    if (lowestSaddleOwner != Node::Null) {
      reversePath(peakId, lowestSaddleOwner);
      // Remember runoff pointing at this peak; will be used in on-map prominence
      mNodes[peakId].runoffId = runoffIndex;
      mNodes[peakId].parentId = Node::Null;  // peak is now root
    }
  }
}

void LineTree::computeOnMapSaddleProminence() {
  // Put peaks in decreasing elevation order
  vector<int> sortedPeakIndices;
  for (int i=0; i < (int) mDivideTree.peaks().size(); ++i) {
    sortedPeakIndices.push_back(i);
  }
  std::sort(sortedPeakIndices.begin(), sortedPeakIndices.end(),
            [this](const int index1, const int index2) {
              return getPeak(index1 + 1).elevation > getPeak(index2 + 1).elevation;
            });

  // Go through peaks in order of decreasing elevation
  for (int index = 0; index < (int) sortedPeakIndices.size(); ++index) {
    // Find this peak's first higher ancestor in divide tree, and
    // lowest intervening saddle on the way there
    Elevation lowestSaddleElevation = HUGE_ELEVATION;
    int lowestSaddleOwner = Node::Null;

    int startingPeakId = sortedPeakIndices[index] + 1;
    int nodeId = startingPeakId;
    Node *node = &mNodes[nodeId];
    int runoffIndex = Node::Null;
    node->childId = Node::Null;
    
    VLOG(3) << "Processing peak " << startingPeakId;
    do {
      node = &mNodes[nodeId];

      // Reached top of tree?
      if (node->parentId == Node::Null) {
        if (node->runoffId == Node::Null) {
          lowestSaddleOwner = nodeId;
        } else {
          // There's a runoff next to this peak
          runoffIndex = node->runoffId;
          const Runoff &runoff = getRunoff(runoffIndex);
          if (runoff.elevation < lowestSaddleElevation) {
            lowestSaddleOwner = nodeId;
            lowestSaddleElevation = runoff.elevation;
          }
        }
        break;
      }

      node->lowestElevationSaddleChildDir = lowestSaddleElevation;
      node->lowestElevationSaddleParentDir = -HUGE_ELEVATION;
      mNodes[node->parentId].childId = nodeId;

      Elevation saddleElevation = getSaddleForPeakId(node->saddleId).elevation;
      if (saddleElevation < lowestSaddleElevation) {
        lowestSaddleOwner = nodeId;
        lowestSaddleElevation = saddleElevation;
      }

      nodeId = node->parentId;

      // Stop if parent is higher in elevation
    } while (getPeak(nodeId).elevation < getPeak(startingPeakId).elevation);

    if (nodeId != Node::Null) {
      // If we hit a runoff at the top of the tree, lowest saddle has
      // unbounded prominence, since its associated peak could be off
      // the map.
      if (runoffIndex == Node::Null) {
        mNodes[nodeId].lowestElevationSaddleParentDir = HUGE_ELEVATION;
      } else {
        mNodes[nodeId].lowestElevationSaddleParentDir = getRunoff(runoffIndex).elevation;
      }
      propagateLowestInterveningSaddle(nodeId);
      
      for (int nid = startingPeakId; nid != nodeId; nid = mNodes[nid].parentId) {
        int saddleOwnerId = mNodes[nid].saddleId;
        Node *saddleOwnerNode = &mNodes[saddleOwnerId];
        lowestSaddleElevation = std::min(mNodes[nid].lowestElevationSaddleChildDir,
                                         mNodes[mNodes[nid].parentId].lowestElevationSaddleParentDir);
        Elevation saddleElevation = getSaddleForPeakId(saddleOwnerId).elevation;
        SaddleInfo *saddleInfo = &mSaddleInfo[getDivideTreeNode(saddleOwnerId).saddleId - 1];
        if (saddleElevation <= lowestSaddleElevation &&
            saddleInfo->saddleProminence == UNDEFINED_ELEVATION) {
          saddleInfo->saddleProminence = getPeak(startingPeakId).elevation - saddleElevation;
          VLOG(3) << "For peak " << startingPeakId << " and owner " << saddleOwnerId
                  << ", setting saddle prom for saddle "
                  << getDivideTreeNode(saddleOwnerId).saddleId << " to " << saddleInfo->saddleProminence;
        }
      }
    }

    // Make parent pointers go from lowest saddle up to this peak.  Point
    // our parent at first higher peak.
    if (startingPeakId != nodeId) {  // Can happen if node is top of tree
      reversePath(startingPeakId, lowestSaddleOwner);
      mNodes[startingPeakId].parentId = nodeId;

      // This very rare case can happen if the top of the tree is next to a low
      // runoff, making the root and the saddle owner the same.  Here the normal
      // code path produces a cycle, which we break here by restoring the top
      // of the tree.
      if (nodeId == lowestSaddleOwner) {
        mNodes[nodeId].parentId = Node::Null;
      }
    }
  }
}

bool LineTree::saddleHasMinProminence(int saddleId, Elevation minProminence) {
  VLOG(3) << "Saddle prom for saddle " << saddleId << " is " << mSaddleInfo[saddleId - 1].saddleProminence;
  return mSaddleInfo[saddleId - 1].saddleProminence >= minProminence;
}

void LineTree::reversePath(int startPeakId, int endPeakId) {
  if (startPeakId != endPeakId) {
    int saddleOwnerId = mNodes[startPeakId].saddleId;
    int peakId = startPeakId;

    mNodes[startPeakId].saddleId = mNodes[endPeakId].saddleId;
    int parentId = mNodes[startPeakId].parentId;
    while (peakId != endPeakId) {
      int grandparentId = mNodes[parentId].parentId;
      VLOG(3) << "Pointing " << parentId << " at " << peakId;
      mNodes[parentId].parentId = peakId;
      int temp = mNodes[parentId].saddleId;
      mNodes[parentId].saddleId = saddleOwnerId;

      peakId = parentId;
      parentId = grandparentId;
      saddleOwnerId = temp;
    }
  }
}

void LineTree::propagateLowestInterveningSaddle(int originNodeId) {
  int nodeId = originNodeId;
  Elevation elev = mNodes[nodeId].lowestElevationSaddleParentDir;
  while (true) {
    int neighborId = mNodes[nodeId].childId;
    if (neighborId == Node::Null) {
      break;
    }
    int saddleOwnerPeakId = (neighborId == getDivideTreeNode(nodeId).parentId) ? nodeId : neighborId;
    Elevation saddleElevation = getSaddleForPeakId(saddleOwnerPeakId).elevation;
    elev = std::min(elev, saddleElevation);
    if (elev <= mNodes[neighborId].lowestElevationSaddleParentDir) {
      break;
    }
    mNodes[neighborId].lowestElevationSaddleParentDir =
      std::max(mNodes[neighborId].lowestElevationSaddleParentDir, elev);
    nodeId = neighborId;
  }
}

const Peak &LineTree::getPeak(int peakId) const {
  return mDivideTree.peaks()[peakId - 1];  // 1-indexed
}

const Saddle &LineTree::getSaddle(int saddleId) const {
  return mDivideTree.saddles()[saddleId - 1];  // 1-indexed
}

const Runoff &LineTree::getRunoff(int runoffId) const {
  return mDivideTree.runoffs()[runoffId];
}

const DivideTree::Node &LineTree::getDivideTreeNode(int nodeId) const {
  return mDivideTree.nodes()[nodeId];
}

const Saddle &LineTree::getSaddleForPeakId(int peakId) const {
  return getSaddle(getDivideTreeNode(peakId).saddleId);
}

int LineTree::peakIdForRunoff(int runoffId) const {
  return mDivideTree.runoffEdges()[runoffId];
}

const vector<LineTree::Node> &LineTree::nodes() const {
  return mNodes;
}

bool LineTree::hasCycle(int maxLength) const {
  for (int i = 1; i < (int) mNodes.size(); ++i) {
    const Node *node = &mNodes[i];

    int length = 0;
    while (node->parentId != Node::Null && length <= maxLength) {
      if (node->parentId == i) {
        printf("Found cycle starting at %d\n", i);
        const Node *cycle = &mNodes[i];
        while (cycle->parentId != i) {
          printf("Parent ID is %d\n", cycle->parentId);
          cycle = &mNodes[cycle->parentId];
        }
        return true;
      }
      
      node = &mNodes[node->parentId];
      length += 1;
    }
  }

  return false;
}

