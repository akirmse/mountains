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

#include "island_tree.h"
#include "divide_tree.h"
#include "easylogging++.h"
#include "kml_writer.h"

#include <assert.h>

using std::string;
using std::vector;

IslandTree::IslandTree(const DivideTree &divideTree) :
    mDivideTree(divideTree) {
}

void IslandTree::build() { 
  // Initialize topology
  mNodes.resize(mDivideTree.nodes().size());
  int index = 1;  // Peaks are 1-indexed; put in a dummy node 0
  for (int index = 1; index < (int) mNodes.size(); ++index) {
    // Copy parent links from divide tree
    mNodes[index].parentId = mDivideTree.nodes()[index].parentId;

    // Initially, we're the only peak on our prominence island
    mNodes[index].saddlePeakId = index;
    mNodes[index].keySaddleId = Node::Null;
    mNodes[index].prominence = Node::UnknownProminence;
  }

  // Now rearrange the topology, pushing higher peaks up the tree
  uninvertPeaks();
  uninvertSaddles();
  
  computeProminences();
}

// Sort peaks so that parent is always higher elevation.
// Set saddlePeakId to indicate highest saddle among parent + children, i.e.
// the highest saddle on the border of the prominence island.
void IslandTree::uninvertPeaks() {
  for (int i = 1; i < (int) mNodes.size(); ++i) {
    uninvertPeak(i);
  }
}

void IslandTree::uninvertPeak(int nodeId) {
  VLOG(3) << "Uninverting peak " << nodeId;
  Node *childNode = &mNodes[nodeId];
  Elevation elev = getPeak(nodeId).elevation;
  int parentId = childNode->parentId;
  while (parentId != Node::Null) {
    // Stop when parent is higher than us.
    if (point2IsHigher(elev, nodeId, getPeak(parentId).elevation, parentId)) {
      break;
    }
    uninvertPeak(parentId);
    
    Node *parentNode = &mNodes[parentId];
    int grandparentId = parentNode->parentId;
    int childSaddlePeakId = childNode->saddlePeakId;
    int parentSaddlePeakId = parentNode->saddlePeakId;

    int childSaddleId = mDivideTree.nodes()[childSaddlePeakId].saddleId;
    int parentSaddleId = mDivideTree.nodes()[parentSaddlePeakId].saddleId;
    if (grandparentId == Node::Null ||
        point2IsHigher(getSaddle(parentSaddleId).elevation, parentSaddleId,
                       getSaddle(childSaddleId).elevation, childSaddleId)) {
      // Move parent node under child node
      parentNode->parentId = nodeId;
      parentNode->saddlePeakId = childSaddlePeakId;
      childNode->saddlePeakId = parentSaddlePeakId;
    }
    
    // Move child up one spot in tree
    VLOG(3) << "Changing parent id of " << nodeId << " from " << parentId
            << " to " << grandparentId;
    assert(nodeId != grandparentId);
    childNode->parentId = grandparentId;

    parentId = grandparentId;
  }
}

void IslandTree::uninvertSaddles() {
  for (int i = 1; i < (int) mNodes.size(); ++i) {
    uninvertSaddle(i);
  }
}

void IslandTree::uninvertSaddle(int nodeId) {
  Node *childNode = &mNodes[nodeId];
  while (true) {
    int parentId = childNode->parentId;
    if (parentId == Node::Null) {
      return;
    }
    Node *parentNode = &mNodes[parentId];
    int grandparentId = parentNode->parentId;
    if (grandparentId == Node::Null) {
      return;
    }

    int childSaddlePeakId = childNode->saddlePeakId;
    int parentSaddlePeakId = parentNode->saddlePeakId;
    int childSaddleId = mDivideTree.nodes()[childSaddlePeakId].saddleId;
    int parentSaddleId = mDivideTree.nodes()[parentSaddlePeakId].saddleId;
    if (point2IsHigher(getSaddle(parentSaddleId).elevation, parentSaddleId,
                       getSaddle(childSaddleId).elevation, childSaddleId)) {
      return;
    }

    uninvertSaddle(parentId);

    // Move up one spot in the tree
    childNode->parentId = grandparentId;
  }
}

void IslandTree::computeProminences() {
  for (int i = 1; i < (int) mNodes.size(); ++i) {
    int elev = getPeak(i).elevation;
    int childNodeId = i;
    int parentNodeId = mNodes[i].parentId; 
    // Find first higher peak in parent chain
    //
    // We need an unambiguous total ordering of peaks, even among
    // those with the same elevation.  When two peaks have the same
    // elevation, we need only one of them to claim a saddle between
    // them as its key saddle.  Without this disambiguation, two peaks
    // could both claim the same saddle as their key saddles.  This
    // would still give correct prominence values, but it interferes
    // with other operations (like pruning the divide tree based on
    // removing key saddles).
    while (parentNodeId != Node::Null) {
      if (point2IsHigher(elev, childNodeId, getPeak(parentNodeId).elevation, parentNodeId)) {
        break;
      }
        
      childNodeId = parentNodeId;
      parentNodeId = mNodes[childNodeId].parentId; 
    }

    if (parentNodeId == Node::Null) {
      // This is the highest point in the tree
      mNodes[i].prominence = elev;
    } else {
      // Prominence = peak elevation - key col elevation
      int saddlePeakId = mNodes[childNodeId].saddlePeakId;
      int saddleId = mDivideTree.nodes()[saddlePeakId].saddleId;
      mNodes[i].prominence = elev - getSaddle(saddleId).elevation;
      mNodes[i].keySaddleId = saddleId;
    }
  }
}

const Peak &IslandTree::getPeak(int peakId) const {
  return mDivideTree.peaks()[peakId - 1];  // 1-indexed
}

const Saddle &IslandTree::getSaddle(int saddleId) const {
  return mDivideTree.saddles()[saddleId - 1];  // 1-indexed
}

const vector<IslandTree::Node> &IslandTree::nodes() const {
  return mNodes;
}

bool IslandTree::point2IsHigher(int point1Elevation, int point1Id, int point2Elevation, int point2Id) {
  return point1Elevation < point2Elevation ||
    (point1Elevation == point2Elevation && point1Id < point2Id);
}

bool IslandTree::writeToFile(const std::string &filename) const {
  FILE *file = fopen(filename.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }

  for (int i = 1; i < (int) mNodes.size(); ++i) {
    const Node &node = mNodes[i];
    fprintf(file, "%d,%d,%d,%d\n", i, node.parentId, node.saddlePeakId, node.keySaddleId);
  }

  fclose(file);
  return true;
}

string IslandTree::getAsKml() const {
  KMLWriter writer(mDivideTree.coordinateSystem());

  // Prominence parent edges
  writer.startFolder("Prominence parent");
  int index = 0;
  for (const Node &node : mNodes) {
    if (node.parentId != Node::Null) {
      writer.addPeakEdge(getPeak(index), getPeak(node.parentId));
    }
    index += 1;
  }  
  writer.endFolder();

  // Island parent edges
  writer.startFolder("Island parent edges");
  index = 0;
  for (const Node &node : mNodes) {
    writer.addPeakEdge(getPeak(index), getPeak(node.saddlePeakId));
    index += 1;
  }
  writer.endFolder();

  // Peaks
  writer.startFolder("Peaks");
  for (index = 1; index < (int) mNodes.size(); ++index) {
    writer.addPeak(getPeak(index), std::to_string(index));
  }
  writer.endFolder();
  
  return writer.finish();
}
