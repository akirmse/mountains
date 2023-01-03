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

#include "divide_tree.h"
#include "coordinate_system.h"
#include "easylogging++.h"
#include "island_tree.h"
#include "kml_writer.h"
#include "line_tree.h"
#include "util.h"

#include <assert.h>
#include <fstream>
#include <unordered_map>

using std::make_pair;
using std::multimap;
using std::string;
using std::unordered_map;
using std::unordered_multimap;
using std::unordered_set;
using std::vector;

// TODO: What to do at boundary of different resolutions?
// TODO: Better KML icons
// TODO: Investigate multithreading crash

DivideTree::DivideTree(const CoordinateSystem &coordinateSystem,
                       const std::vector<Peak> &peaks, const std::vector<Saddle> &saddles,
                       const std::vector<Runoff> &runoffs) :
    // Copy arrays
    mPeaks(peaks), mSaddles(saddles), mRunoffs(runoffs) {
  mCoordinateSystem = std::unique_ptr<CoordinateSystem>(coordinateSystem.clone());
  mNodes.resize(mPeaks.size() + 1);  // Peaks are 1-indexed; put in a dummy node 0
  for (Node &node : mNodes) {
    node.parentId = Node::Null;
    node.saddleId = Node::Null;
  }
  mRunoffEdges.resize(mRunoffs.size());  // Runoffs are 0-indexed
}

int DivideTree::maybeAddEdge(int peakId1, int peakId2, int saddleId) {
  // If peaks are already in the same tree, new edge would create a cycle.
  int commonAncestorId = findCommonAncestor(peakId1, peakId2);
  if (commonAncestorId == Node::Null) {
    // Two separate trees; fine to add edge.
    // Make one peak into the root of its tree so that we can
    // set its parent to the other peak.
    makeNodeIntoRoot(peakId1);
    mNodes[peakId1].parentId = peakId2;
    mNodes[peakId1].saddleId = saddleId;
    VLOG(3) << "Adding divide tree edge " << peakId1 << " " << peakId2;
    return Node::Null;
  }

  // Find lowest saddle on proposed cycle
  int lowestSaddleNode1 = findLowestSaddleOnPath(peakId1, commonAncestorId);
  int lowestSaddleNode2 = findLowestSaddleOnPath(peakId2, commonAncestorId);

  VLOG(3) << "Common ancestor is " << commonAncestorId; 
  VLOG(3) << "Low saddle candidates are " << lowestSaddleNode1 << " " << lowestSaddleNode2;
  
  // Make node1 the one with a guaranteed parent and saddle
  if (lowestSaddleNode1 == Node::Null || mNodes[lowestSaddleNode1].saddleId == Node::Null) {
    std::swap(lowestSaddleNode1, lowestSaddleNode2);
  }

  assert(lowestSaddleNode1 != Node::Null);
  assert(mNodes[lowestSaddleNode1].saddleId != Node::Null);
  
  Elevation lowestSaddleElevation = getSaddle(mNodes[lowestSaddleNode1].saddleId).elevation;
  int lowestSaddleNodeId = lowestSaddleNode1;
  if (lowestSaddleNodeId == Node::Null ||
      (lowestSaddleNode2 != Node::Null && getSaddle(mNodes[lowestSaddleNode2].saddleId).elevation < lowestSaddleElevation)) {
    lowestSaddleElevation = getSaddle(mNodes[lowestSaddleNode2].saddleId).elevation;
    lowestSaddleNodeId = lowestSaddleNode2;
  }

  // If proposed saddle is the lowest, discard new edge, nothing to do
  if (getSaddle(saddleId).elevation < lowestSaddleElevation) {
    return saddleId;
  }

  // Break edge with lowest saddle
  int basinSaddleId = mNodes[lowestSaddleNodeId].saddleId;
  mNodes[lowestSaddleNodeId].parentId = Node::Null;
  mNodes[lowestSaddleNodeId].saddleId = Node::Null;
  
  // Add new edge
  makeNodeIntoRoot(peakId1);
  mNodes[peakId1].parentId = peakId2;
  VLOG(3) << "Adding modified divide tree edge " << peakId1 << " " << peakId2;
  mNodes[peakId1].saddleId = saddleId;

  return basinSaddleId;
}

void DivideTree::addRunoffEdge(int peakId, int runoffId) {
  mRunoffEdges[runoffId] = peakId;
}

void DivideTree::prune(Elevation minProminence, const IslandTree &islandTree) {
  // We'll need line tree to know whether it's safe to delete saddles
  LineTree lineTree(*this);
  lineTree.build();
  
  unordered_set<int> deletedPeakIndices;  // 0-based
  unordered_set<int> deletedSaddleIndices;  // 0-based
  
  // Build up back references to peaks.
  // map of peak ID to all peaks just above and below in tree ("neighbors").
  multimap<int, int> neighbors;
  for (int peakId = 1; peakId < (int) mNodes.size(); ++peakId) {
    const Node &node = mNodes[peakId];
    if (node.parentId != Node::Null) {
      neighbors.insert(make_pair(node.parentId, peakId));
      neighbors.insert(make_pair(peakId, node.parentId));
    }
  }
  
  // map of peakID to runoffIDs that point to it
  multimap<int, int> runoffNeighbors;
  for (int runoffId = 0; runoffId < (int) mRunoffEdges.size(); ++runoffId) {
    runoffNeighbors.insert(make_pair(mRunoffEdges[runoffId], runoffId));
  }

  // Look for low-prominence peaks whose highest saddles also have low
  // prominence (as measured in the line tree). Keep removing such
  // peaks until nothing gets removed.  Subsequent loops may remove
  // more peaks because saddles will get wired up differently as
  // peaks are removed.
  bool anythingChanged = true;
  while (anythingChanged) {
    anythingChanged = false;

    VLOG(3) << "Looping over peaks looking for low prominence to prune";
    
    for (int peakId = 1; peakId < (int) mNodes.size(); ++peakId) {
      const Peak &peak = getPeak(peakId);
      const Node &node = mNodes[peakId];
      // Peak has below min prominence?
      const IslandTree::Node &iNode = islandTree.nodes()[peakId];
      if (deletedPeakIndices.find(peakId - 1) == deletedPeakIndices.end() &&
          iNode.prominence != IslandTree::Node::UnknownProminence &&
          iNode.prominence < minProminence) {
        const auto range = neighbors.equal_range(peakId);
        if (range.first == range.second) {
          // No neighbors; isolated peak.  If not connected to runoff,
          // just nuke it.  If it is connected to a runoff, we have to
          // keep it, because it might have high prominence from a
          // neighboring tile.
          if (runoffNeighbors.find(peakId) == runoffNeighbors.end()) {
            VLOG(3) << "Removing isolated peak " << peakId;
            deletedPeakIndices.insert(peakId - 1);
            anythingChanged = true;
          }
          continue;
        }

        // Safe to delete peak only if our highest saddle doesn't have min prominence.
        bool deletePeak = false;
        int ownerOfSaddleToDelete = Node::Null;
        Elevation highestSaddleElevation = 0;
        for (auto it = range.first; it != range.second; ++it) {
          int neighborPeakId = it->second;
          int saddleOwnerPeakId = (neighborPeakId == node.parentId) ? peakId : neighborPeakId;
          const Saddle &saddle = getSaddle(mNodes[saddleOwnerPeakId].saddleId);
          if (ownerOfSaddleToDelete == Node:: Null || saddle.elevation > highestSaddleElevation) {
            ownerOfSaddleToDelete = saddleOwnerPeakId;
            highestSaddleElevation = saddle.elevation;
          }
        }
        if (ownerOfSaddleToDelete != Node::Null) {
          int saddleId = mNodes[ownerOfSaddleToDelete].saddleId;
          deletePeak = !lineTree.saddleHasMinProminence(saddleId, minProminence);
        }

        if (deletePeak) {
          int saddleIdToDelete = mNodes[ownerOfSaddleToDelete].saddleId;
          VLOG(3) << "Pruning peak " << peakId << " with saddle owner "
                  << ownerOfSaddleToDelete << " saddle " << saddleIdToDelete << " and prominence " << iNode.prominence;
          // Skip over eliminated saddle
          int saddleParentId = mNodes[ownerOfSaddleToDelete].parentId;
          mNodes[ownerOfSaddleToDelete].saddleId = mNodes[saddleParentId].saddleId;
          
          // Skip over eliminated peak: all our children must point to a new parent
          int newParentId = node.parentId;
          // If saddle is owned by this peak, new parent is our
          // parent, otherwise new parent is saddle owner (one of our
          // children).
          if (peakId != ownerOfSaddleToDelete) {
            newParentId = ownerOfSaddleToDelete;
            mNodes[ownerOfSaddleToDelete].parentId = node.parentId;
          }
          vector<int> neighborIds;
          for (auto it = range.first; it != range.second; ++it) {
            int neighborPeakId = it->second;
            neighborIds.push_back(neighborPeakId);
            if (neighborPeakId != node.parentId && neighborPeakId != newParentId) {
              mNodes[neighborPeakId].parentId = newParentId;
            }
          }
          
          // Update neighbors
          for (auto neighborPeakId : neighborIds) {
            removeFromMultimap(&neighbors, neighborPeakId, peakId);
            if (neighborPeakId != newParentId) {              
              neighbors.insert(make_pair(newParentId, neighborPeakId));
              neighbors.insert(make_pair(neighborPeakId, newParentId));
            }
          }

          // Any runoffs pointing to us must point to our parent
          const auto runoffRange = runoffNeighbors.equal_range(peakId);
          vector<int> runoffIds;
          for (auto it = runoffRange.first; it != runoffRange.second; ++it) {
            runoffIds.push_back(it->second);
          }
          for (auto runoffId : runoffIds) {
            mRunoffEdges[runoffId] = newParentId;
            runoffNeighbors.insert(make_pair(newParentId, runoffId));
            // Runoff's adjacent peak is gone; it can't have any influence on the new parent
            mRunoffs[runoffId].insidePeakArea = false;
          }
          
          mNodes[peakId].parentId = Node::Null;
          mNodes[peakId].saddleId = Node::Null;
          neighbors.erase(peakId);
          runoffNeighbors.erase(peakId);
          deletedPeakIndices.insert(peakId - 1);
          deletedSaddleIndices.insert(saddleIdToDelete - 1);
          anythingChanged = true;
        }
      }
    }
  }

  removeDeletedPeaksAndSaddles(deletedPeakIndices, deletedSaddleIndices);
  VLOG(1) << "Pruned to " << mPeaks.size() << " peaks and " << mSaddles.size() << " saddles";
}

void DivideTree::merge(const DivideTree &otherTree) {
  int oldNumPeaks = static_cast<int>(mPeaks.size());
  int oldNumSaddles = static_cast<int>(mSaddles.size());
  int oldNumNodes = static_cast<int>(mNodes.size());
  int oldNumRunoffs = static_cast<int>(mRunoffs.size());
                                    
  // Glue arrays together
  mPeaks.insert(mPeaks.end(), otherTree.mPeaks.begin(), otherTree.peaks().end());
  mSaddles.insert(mSaddles.end(), otherTree.saddles().begin(), otherTree.saddles().end());
  mRunoffs.insert(mRunoffs.end(), otherTree.runoffs().begin(), otherTree.runoffs().end());
  // Skip first, empty node
  mNodes.insert(mNodes.end(), otherTree.nodes().begin() + 1, otherTree.nodes().end());
  mRunoffEdges.insert(mRunoffEdges.end(),
                      otherTree.mRunoffEdges.begin(), otherTree.mRunoffEdges.end());

  // Patch up references in new nodes
  for (int i = oldNumNodes; i < (int) mNodes.size(); ++i) {
    Node *node = &mNodes[i];
    if (node->parentId != Node::Null) {
      node->parentId += oldNumPeaks;
    }
    if (node->saddleId != Node::Null) {
      node->saddleId += oldNumSaddles;
    }
  }

  // Patch up references in new runoffs
  for (int i = oldNumRunoffs; i < (int) mRunoffEdges.size(); ++i) {
    mRunoffEdges[i] += oldNumPeaks;
  }

  // Actually connect the two subtrees
  spliceAllRunoffs();
}

bool DivideTree::setOrigin(const CoordinateSystem &coordinateSystem) {
  if (!mCoordinateSystem->compatibleWith(coordinateSystem)) {
    LOG(ERROR) << "Can't merge divide trees with different pixel sizes";
    return false;
  }

  Offsets offsets(mCoordinateSystem->offsetsTo(coordinateSystem));
  int dx = offsets.x();
  int dy = offsets.y();
  VLOG(2) << "Offsetting origin by " << dx << " " << dy;
  for (Peak &peak : mPeaks) {
    peak.location = peak.location.offsetBy(dx, dy);
  }
  for (Saddle &saddle : mSaddles) {
    saddle.location = saddle.location.offsetBy(dx, dy);
  }
  for (Runoff &runoff : mRunoffs) {
    runoff.location = runoff.location.offsetBy(dx, dy);
  }

  mCoordinateSystem.reset(coordinateSystem.clone());
  return true;
}

void DivideTree::compact() {
  unordered_set<int> removedIndices;
  unordered_set<int> emptyIndices;

  // TODO: May want to save basin saddles for debugging, only delete during merge
  for (int i = 0; i < (int) mSaddles.size(); ++i) {
    const Saddle &saddle = mSaddles[i];
    if (saddle.type == Saddle::Type::ERROR_SADDLE ||
        saddle.type == Saddle::Type::FALSE_SADDLE ||
        saddle.type == Saddle::Type::BASIN) {
      removedIndices.insert(i);
    }
  }

  removeDeletedPeaksAndSaddles(emptyIndices, removedIndices);
}

void DivideTree::deleteRunoffs() {
  mRunoffs.clear();
  mRunoffEdges.clear();
}

void DivideTree::flipElevations() {
  for (Peak &peak : mPeaks) {
    peak.elevation = -peak.elevation;
  }
  for (Saddle &saddle : mSaddles) {
    saddle.elevation = -saddle.elevation;
  }
  for (Runoff &runoff : mRunoffs) {
    runoff.elevation = -runoff.elevation;
  }
}

bool DivideTree::writeToFile(const std::string &filename) const {
  FILE *file = fopen(filename.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }

  fprintf(file, "# Prominence divide tree generated at %s\n", getTimeString().c_str());

  // First line describes coordinate system
  fprintf(file, "%s\n", mCoordinateSystem->toString().c_str());

  int index = 1;
  for (const Peak &peak : mPeaks) {
    fprintf(file, "P,%d,%d,%d,%.2f\n", index++, peak.location.x(), peak.location.y(),
            peak.elevation);
  }

  index = 1;
  for (const Saddle &saddle : mSaddles) {
    fprintf(file, "S,%d,%c,%d,%d,%.2f\n", index++, static_cast<char>(saddle.type),
            saddle.location.x(), saddle.location.y(), saddle.elevation);
  }

  index = 0;
  for (const Runoff &runoff : mRunoffs) {
    fprintf(file, "R,%d,%d,%d,%.2f,%d,%d\n", index++, runoff.location.x(), runoff.location.y(),
            runoff.elevation,
            runoff.filledQuadrants, runoff.insidePeakArea ? 1 : 0);
  }

  index = 0;
  for (const Node &node : mNodes) {
    fprintf(file, "N,%d,%d,%d\n", index++, node.parentId, node.saddleId);
  }

  index = 0;
  for (int peakId : mRunoffEdges) {
    fprintf(file, "E,%d,%d\n", index++, peakId);
  }

  fclose(file);
  return true;
}

DivideTree *DivideTree::readFromFile(const std::string &filename) {
  if (!fileExists(filename)) {
    return nullptr;
  }
  
  std::ifstream file(filename);

  vector<Peak> peaks;
  vector<Saddle> saddles;
  vector<Runoff> runoffs;
  vector<Node> nodes;
  vector<int> runoffEdges;
  Node node;

  bool coordinateSystemRead = false;
  std::unique_ptr<CoordinateSystem> coordinateSystem;
  string line;
  vector<string> elements;
  while (file.good()) {
    std::getline(file, line);
    // Skip blank lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Coordinate system is first real line
    if (!coordinateSystemRead) {
      auto coords = CoordinateSystem::fromString(line);
      if (coords == nullptr) {
        LOG(ERROR) << "Missing valid coordinate system description line";
        return nullptr;
      }
      coordinateSystem.reset(coords);
      coordinateSystemRead = true;
      continue;
    }

    split(line, ',', elements);
    switch (elements[0][0]) {
    case 'P':
      if (elements.size() != 5) {
        return nullptr;
      }
      peaks.push_back(Peak(Offsets(stoi(elements[2]), stoi(elements[3])),
                           stof(elements[4])));
      break;
    case 'S': {
      if (elements.size() != 6) {
        return nullptr;
      }
      Saddle saddle(Offsets(stoi(elements[3]), stoi(elements[4])), stof(elements[5]));
      saddle.type = Saddle::typeFromChar(elements[2][0]);
      saddles.push_back(saddle);
      break;
    }
    case 'R': {
      if (elements.size() != 7) {
        return nullptr;
      }
      Runoff runoff(Offsets(stoi(elements[2]), stoi(elements[3])), stof(elements[4]),
                    stoi(elements[5]));
      runoff.insidePeakArea = (elements[6] == "1");
      runoffs.push_back(runoff);
      break;
    }
    case 'N':
      if (elements.size() != 4) {
        return nullptr;
      }
      node.parentId = stoi(elements[2]);
      node.saddleId = stoi(elements[3]);
      if (node.parentId != Node::Null && node.saddleId == Node::Null) {
        LOG(ERROR) << "Bad saddle ID " << node.saddleId;
      } else {
        nodes.push_back(node);
      }
      break;
    case 'E':
      if (elements.size() != 3) {
        return nullptr;
      }
      runoffEdges.push_back(stoi(elements[2]));
    }
  }

  DivideTree *tree = new DivideTree(*coordinateSystem, peaks, saddles, runoffs);
  tree->mNodes = nodes;
  tree->mRunoffEdges = runoffEdges;
  return tree;
}

int DivideTree::findLowestSaddleOnPath(int childPeakId, int ancestorPeakId) {
  if (childPeakId == ancestorPeakId) {
    return Node::Null;
  }
  
  int lowestSaddleNodeId = childPeakId;
  while (childPeakId != ancestorPeakId) {
    int parentPeakId = mNodes[childPeakId].parentId;
    if (parentPeakId == Node::Null) {
      LOG(ERROR) << "Couldn't find a path from node " << childPeakId << " to node " << ancestorPeakId;
      return Node::Null;
    }

    // Lower saddle?
    int childSaddleId = mNodes[childPeakId].saddleId;
    if (getSaddle(childSaddleId).elevation < getSaddle(mNodes[lowestSaddleNodeId].saddleId).elevation) {
      lowestSaddleNodeId = childPeakId;
    }

    childPeakId = parentPeakId;
  }

  return lowestSaddleNodeId;
}

void DivideTree::makeNodeIntoRoot(int nodeId) {
  // Reverse all the parent links leading from this node up to current
  // root, moving saddles to the reversed links.
  Node *childNode = &mNodes[nodeId];
  int childId = nodeId;
  int parentId = childNode->parentId;
  int saddleId = childNode->saddleId;

  while (parentId != Node::Null) {
    Node *parentNode = &mNodes[parentId];
    int grandparentId = parentNode->parentId;
    int tempSaddleId = parentNode->saddleId;
    parentNode->saddleId = saddleId;
    parentNode->parentId = childId;
    saddleId = tempSaddleId;
   
    childNode = parentNode;
    childId = parentId;
    parentId = grandparentId;
  }

  // Specified node is now root of tree; clear its parent
  childNode = &mNodes[nodeId];
  childNode->saddleId = Node::Null;
  childNode->parentId = Node::Null;
}

int DivideTree::findCommonAncestor(int nodeId1, int nodeId2) {
  // First go up tree on one side until both nodes are at the same depth
  int depth1 = getDepth(nodeId1);
  int depth2 = getDepth(nodeId2);

  while (depth1 > depth2) {
    nodeId1 = mNodes[nodeId1].parentId;
    if (nodeId1 == Node::Null) {
      break;
    }
    depth1 -= 1;
  }
  
  while (depth2 > depth1) {
    nodeId2 = mNodes[nodeId2].parentId;
    if (nodeId2 == Node::Null) {
      break;
    }
    depth2 -= 1;
  }

  // Walk up one level at a time, looking for common ancestor
  while (true) {
    if (nodeId1 == Node::Null || nodeId2 == Node::Null) {
      return Node::Null;
    }

    if (nodeId1 == nodeId2) {
      return nodeId1;
    }

    nodeId1 = mNodes[nodeId1].parentId;
    nodeId2 = mNodes[nodeId2].parentId;
  }
}

int DivideTree::getDepth(int nodeId) {
  int depth = 0;

  do {
    depth += 1;
    nodeId = mNodes[nodeId].parentId;
  } while (nodeId != Node::Null);
  
  return depth;
}

void DivideTree::spliceAllRunoffs() {
  unordered_set<int> removedRunoffs;

  int samplesAroundGlobe = mCoordinateSystem->samplesAroundEquator();

  // When there are lots of runoffs, an N^2 lookup gets slow: use a hash map to make it O(N).
  unordered_multimap<Offsets::Value, int> locationMap;  // map of location to runoff index
  for (int i = 0; i < (int) mRunoffs.size(); ++i) {
    locationMap.insert(make_pair(mRunoffs[i].location.value(), i));
  }

  // Look for two non-deleted runoffs directly on top of each other
  for (int i = 0; i < (int) mRunoffs.size(); ++i) {
    if (removedRunoffs.find(i) == removedRunoffs.end()) {
      // Watch for wrapping around antimeridian: try +/- 360 degrees longitude, too
      Offsets runoffLocation = mRunoffs[i].location;
      for (int wraparound = -1; wraparound <= 1; ++wraparound) {
        Offsets wraparoundLocation(runoffLocation.x() + wraparound * samplesAroundGlobe,
                                    runoffLocation.y());
        const auto range = locationMap.equal_range(wraparoundLocation.value());
        for (auto it = range.first; it != range.second; ++it) {
          int otherRunoffIndex = it->second;
          // Other runoff can't be deleted, and can't be us
          if (otherRunoffIndex != i && removedRunoffs.find(otherRunoffIndex) == removedRunoffs.end()) {
            spliceTwoRunoffs(i, otherRunoffIndex, &removedRunoffs);
            break;
          }
        }
      }
    }
  }

  // Actually remove dead runoffs
  removeVectorElementsByIndices(&mRunoffs, removedRunoffs);
  removeVectorElementsByIndices(&mRunoffEdges, removedRunoffs);

  // Actually remove dead peaks and saddles
  removeDeletedPeaksAndSaddles(mRemovedPeakIndices, mRemovedSaddleIndices);

  mRemovedPeakIndices.clear();
  mRemovedSaddleIndices.clear();
}

void DivideTree::spliceTwoRunoffs(int index1, int index2, unordered_set<int> *removedRunoffs) {
  VLOG(3) << "Splicing runoffs " << index1 << " and " << index2;

  // Runoffs pointing at different peaks?
  int peak1 = mRunoffEdges[index1];
  int peak2 = mRunoffEdges[index2];
  bool wasRunoff1InsidePeakArea = mRunoffs[index1].insidePeakArea;
  bool wasRunoff2InsidePeakArea = mRunoffs[index2].insidePeakArea;
  if (peak1 != peak2) {
    // Make a new saddle at this location, and add edge to tree
    mSaddles.push_back(Saddle(mRunoffs[index1].location, mRunoffs[index1].elevation));
    int basinSaddleId = maybeAddEdge(peak1, peak2, static_cast<int>(mSaddles.size()));
    if (basinSaddleId != DivideTree::Node::Null) {
      mSaddles[basinSaddleId - 1].type = Saddle::Type::BASIN;
    }
    
    // While not strictly required for prominence correctness,
    // it makes the divide tree look cleaner if we remove any
    // false peaks along the tile edge.  If the runoff is inside
    // a peak area, then either the peak should also appear on the
    // other side of the boundary, or it's bogus.  Either way,
    // it's safe to remove one side.
    if (mRunoffs[index1].insidePeakArea) {
      removePeak(mRunoffEdges[index1], mRunoffEdges[index2]);
    } else if (mRunoffs[index2].insidePeakArea) {
      removePeak(mRunoffEdges[index2], mRunoffEdges[index1]);
    }
  }

  // We can definitely remove one of the runoffs.
  removedRunoffs->insert(index1);
  
  // It's OK to remove the other runoff if the two together have
  // seen all neighboring pixels.  Otherwise, keep the runoff
  // for a future merge.
  mRunoffs[index2].filledQuadrants += mRunoffs[index1].filledQuadrants;
  if (mRunoffs[index2].filledQuadrants >= 4) {
    removedRunoffs->insert(index2);
  } else {
    // Combined runoff is in peak area only if both runoffs were
    // (removing peaks may have overwritten values, which is why we have to
    // store them separately in bools)
    mRunoffs[index2].insidePeakArea = wasRunoff2InsidePeakArea && wasRunoff1InsidePeakArea;
  }
}

void DivideTree::removePeak(int peakId, int neighborPeakId) {
  VLOG(3) << "Removing peak " << peakId << " with neighbor " << neighborPeakId;

  // See if one peak is a child of the other.  If so, the saddle
  // between them is the one to remove.
  int removedSaddleId = mNodes[peakId].saddleId;
  if (mNodes[peakId].parentId != neighborPeakId) {
    bool saddleOwnerIsChild = true;
    if (mNodes[neighborPeakId].parentId != peakId) {
      // There isn't a saddle between us and neighbor.  Remove our
      // highest neighboring saddle, which is somewhat expensive to
      // find.
      VLOG(3) << "Rare case of removing peak with no saddle to neighbor";
      Elevation highestSaddleElevation = 0;
      // Saddle to parent?
      if (mNodes[peakId].parentId != Node::Null) {
        neighborPeakId = mNodes[peakId].parentId;
        highestSaddleElevation = getSaddle(mNodes[peakId].saddleId).elevation;
        saddleOwnerIsChild = false;
      }
      // Saddle from child?
      for (int nodeId = 1; nodeId < (int) mNodes.size(); ++nodeId) {
        const Node &node = mNodes[nodeId];
        if (node.parentId == peakId) {
          Elevation elevation = getSaddle(node.saddleId).elevation;
          if (elevation > highestSaddleElevation) {
            highestSaddleElevation = elevation;
            neighborPeakId = nodeId;
            saddleOwnerIsChild = true;
          }
        }
      }

      VLOG(3) << "Now removing peak " << peakId << " with neighbor " << neighborPeakId;
    }

    if (saddleOwnerIsChild) {
      // Saddle owner is child of peakId: preserve previous saddle between peakId and its parent
      removedSaddleId = mNodes[neighborPeakId].saddleId;
      mNodes[neighborPeakId].parentId = mNodes[peakId].parentId;
      mNodes[neighborPeakId].saddleId = mNodes[peakId].saddleId;
    }
  }

  assert(removedSaddleId != Node::Null);
  
  // Mark peak and saddle as dead.
  // It's much faster to mark them dead here and update all indices at once later
  // than to update all the indices for each deleted peak.
  mRemovedPeakIndices.insert(peakId - 1);
  mRemovedSaddleIndices.insert(removedSaddleId - 1);
    
  // Update node pointers
  for (Node &node : mNodes) {
    if (node.parentId == peakId) {
      node.parentId = neighborPeakId;
    }
  }

  // Update runoff edge pointers
  int index = 0;
  for (int &runoffEdgeId : mRunoffEdges) {
    if (runoffEdgeId == peakId) {
      runoffEdgeId = neighborPeakId;

      // When a runoff is pointed at a new peak, we should mark
      // it as no longer inside the flat area of a peak (since
      // that applied only to its old peak).  The flat areas
      // of two peaks obviously can't touch.
      mRunoffs[index].insidePeakArea = false;
    }

    index += 1;
  }
}

const Peak &DivideTree::getPeak(int peakId) const {
  return mPeaks[peakId - 1];  // 1-indexed
}

const Saddle &DivideTree::getSaddle(int saddleId) const {
  return mSaddles[saddleId - 1];  // 1-indexed
}

const CoordinateSystem &DivideTree::coordinateSystem() const {
  return *mCoordinateSystem;
}

const vector<Peak> &DivideTree::peaks() const {
  return mPeaks;
}

const vector<Saddle> &DivideTree::saddles() const {
  return mSaddles;
}

const vector<Runoff> &DivideTree::runoffs() const {
  return mRunoffs;
}

const std::vector<int> &DivideTree::runoffEdges() const {
  return mRunoffEdges;
}

const vector<DivideTree::Node> &DivideTree::nodes() const {
  return mNodes;
}

void DivideTree::setSaddles(const std::vector<Saddle> saddles) {
  mSaddles = saddles;
}

void DivideTree::debugPrint() const {
  int index = 0;
  for (const Node &node : mNodes) {
    if (node.saddleId != Node::Null) {
      printf("  %d -> %d saddle %d\n", index, node.parentId, node.saddleId);
    }
    index += 1;
  }
}

string DivideTree::getAsKml() const {
  KMLWriter writer(*mCoordinateSystem);
  
  // Graph edges
  writer.startFolder("Edges");
  int index = 0;
  for (const Node &node : mNodes) {
    if (node.saddleId != Node::Null) {
      writer.addGraphEdge(getPeak(index), getPeak(node.parentId), getSaddle(node.saddleId));
    }
    index += 1;
  }

  // Runoff edges
  for (int i = 0; i < (int) mRunoffEdges.size(); ++i) {
    const Runoff &runoff = mRunoffs[i];
    if (mRunoffEdges[i] != Node::Null) {
      writer.addRunoffEdge(getPeak(mRunoffEdges[i]), runoff);
    }
  }
  writer.endFolder();
  
  // Peaks
  writer.startFolder("Peaks");
  index = 0;
  for (const Peak &peak : mPeaks) {
    index += 1;
    writer.addPeak(peak, std::to_string(index));
  }
  writer.endFolder();

  // Prom saddles
  writer.startFolder("Prom saddles");
  index = 0;
  for (const Saddle &saddle : mSaddles) {
    index += 1;
    if (saddle.type == Saddle::Type::PROM) {
      writer.addPromSaddle(saddle, std::to_string(index));
    }
  }
  writer.endFolder();

  // Basin saddles
  writer.startFolder("Basin saddles");
  index = 0;
  for (const Saddle &saddle : mSaddles) {
    index += 1;
    if (saddle.type == Saddle::Type::BASIN) {
      writer.addBasinSaddle(saddle, std::to_string(index));
    }
  }
  writer.endFolder();

  // Runoffs
  writer.startFolder("Runoffs");
  index = 0;
  for (const Runoff &runoff : mRunoffs) {
    writer.addRunoff(runoff, std::to_string(index));
    index += 1;
  }
  writer.endFolder();

  return writer.finish();
}

void DivideTree::computeDeletionOffsets(const unordered_set<int> &deletedIndices,
                                        vector<int> &deletionOffsets) const {
  if (deletedIndices.empty()) {
    return;
  }
  
  // deletionOffsets[i] tells how much to subtract to go from
  // pre-deletion index i to post-deletion index.

  vector<int> sortedDeletedIndices(deletedIndices.begin(), deletedIndices.end());
  std::sort(sortedDeletedIndices.begin(), sortedDeletedIndices.end());
  int offset = 1;
  for (int i = 0; i < (int) sortedDeletedIndices.size() - 1; ++i) {
    int deletedIndex = sortedDeletedIndices[i];
    int nextDeletedIndex = sortedDeletedIndices[i + 1];
    for (int index = deletedIndex; index < nextDeletedIndex; ++index) {
      deletionOffsets[index] = offset;
    }
    offset += 1;
  }
  
  // Part of array past the last deleted index
  for (int index = sortedDeletedIndices.back(); index < (int) deletionOffsets.size(); ++index) {
    deletionOffsets[index] = offset;      
  }
}

void DivideTree::removeDeletedPeaksAndSaddles(const std::unordered_set<int> &deletedPeakIndices,
                                              const std::unordered_set<int> &deletedSaddleIndices) {
  // Compute offsets to account for deletions
  vector<int> peakDeletionOffsets(mPeaks.size(), 0);
  computeDeletionOffsets(deletedPeakIndices, peakDeletionOffsets);

  vector<int> saddleDeletionOffsets(mSaddles.size(), 0);
  computeDeletionOffsets(deletedSaddleIndices, saddleDeletionOffsets);

  // Compact peak / saddle / node arrays to deal with deletions
  removeVectorElementsByIndices(&mSaddles, deletedSaddleIndices); 
  removeVectorElementsByIndices(&mPeaks, deletedPeakIndices);
  // Indices are 0 based: need to temporarily remove blank mNodes[0]
  mNodes.erase(mNodes.begin());
  removeVectorElementsByIndices(&mNodes, deletedPeakIndices);
  mNodes.insert(mNodes.begin(), Node());

  // Update indices in nodes and runoff edges to account for deletions
  for (Node &node : mNodes) {
    if (node.parentId != Node::Null) {
      node.parentId -= peakDeletionOffsets[node.parentId - 1];
    }

    if (node.saddleId != Node::Null) {
      node.saddleId -= saddleDeletionOffsets[node.saddleId - 1];
    }
  }
  for (int &edge : mRunoffEdges) {
    if (edge != Node::Null) {
      edge -= peakDeletionOffsets[edge - 1];
    }
  }
}
