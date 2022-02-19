/*
 * MIT License
 * 
 * Copyright (c) 2022 Andrew Kirmse
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

// A tool to compute prominence parents from a divide tree.

#include "divide_tree.h"
#include "island_tree.h"
#include "line_tree.h"
#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif
#ifdef PLATFORM_WINDOWS
#include "getopt-win.h"
#endif

#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

using std::string;
using std::vector;

static void usage() {
  printf("Usage:\n");
  printf("  compute_parents divide_tree.dvt output_file\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -m min_prominence Minimum prominence threshold for output, default = 300ft\n");
  exit(1);
}

int main(int argc, char **argv) {
  int minProminence = 300;

  // Parse options
  START_EASYLOGGINGPP(argc, argv);

  int ch;
  while ((ch = getopt(argc, argv, "m:")) != -1) {
    switch (ch) {
    case 'm':
      minProminence = static_cast<int>(atoi(optarg));
      break;
    }
  }

  argc -= optind;
  argv += optind;
  if (argc < 2) {
    usage();
  }

  // Load divide tree
  string inputFilename = argv[0];
  string outputFilename = argv[1];
  VLOG(1) << "Loading tree from " << inputFilename;
  DivideTree *divideTree = DivideTree::readFromFile(inputFilename);
  if (divideTree == nullptr) {
    LOG(ERROR) << "Failed to load divide tree from " << inputFilename;
    return 1;
  }

  // Divide tree should be a "finalized" one with no runoffs.  We
  // don't handle runoffs in this calculation.
  if (!divideTree->runoffs().empty()) {
    LOG(ERROR) << "Provide a finalized divide tree that has had all runoffs removed\n"
               << "(the -f option to merge_divide_trees)";
    return 1;
  }
  
  // Build island tree to get prominence values
  IslandTree *islandTree = new IslandTree(*divideTree);
  islandTree->build();

  // Build line parent tree
  LineTree *lineTree = new LineTree(*divideTree);
  lineTree->build();

  FILE *file = fopen(outputFilename.c_str(), "wb");
  const CoordinateSystem &coords = divideTree->coordinateSystem();
  const vector<DivideTree::Node> &divideNodes = divideTree->nodes();
  const vector<IslandTree::Node> &islandNodes = islandTree->nodes();
  const vector<LineTree::Node> &lineNodes = lineTree->nodes();
  const vector<Peak> &peaks = divideTree->peaks();
  for (int i = 1; i < (int) divideNodes.size(); ++i) {
    auto prom = islandNodes[i].prominence;
    // Skip dinky peaks
    if (prom < minProminence) {
      continue;
    }

    auto &childPeak = peaks[i - 1];
    LatLng childPos = coords.getLatLng(childPeak.location);

    // No output for landmass high points
    if (prom == childPeak.elevation) {
      continue;
    }
    
    VLOG(3) << "Considering peak " <<
        childPos.latitude() << "," << childPos.longitude() << ",P=" << prom;

    // Walk up line tree, looking for first peak with higher prominence.
    // (Prominence values are stored in island tree.)
    int parentId = lineNodes[i].parentId;
    while (parentId != DivideTree::Node::Null) {
      auto parentProm = islandNodes[parentId].prominence;
      if (parentProm > prom) {
        auto &parentPeak = peaks[parentId - 1];
        LatLng parentPos = coords.getLatLng(parentPeak.location);
        fprintf(file, "%.4f,%.4f,%d,%.4f,%.4f,%d\n",
                childPos.latitude(), childPos.longitude(), prom,
                parentPos.latitude(), parentPos.longitude(), parentProm);
        break;
      }
      
      parentId = lineNodes[parentId].parentId;
    }
    
    if (parentId == DivideTree::Node::Null) {
      VLOG(2) << "No prominence parent for peak " <<
        childPos.latitude() << "," << childPos.longitude() << ",P=" << prom;
    }
  }
  
  delete islandTree;
  delete lineTree;
  delete divideTree;
  
  return 0;
}
