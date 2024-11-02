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

// A tool to compute prominence parents and line parents from a divide tree.

// The output looks like:
// peak lat,peak lng,peak prominence,pparent lat,pparent lng,pparent prom,lparent lat,lparent lng,lparent elevation
//
// Peaks that are landmass high points don't appear in the output, because we
// don't have well-defined parents for them.

#include "divide_tree.h"
#include "island_tree.h"
#include "line_tree.h"
#include "util.h"

#include "easylogging++.h"
#include "getopt_internal.h"

#include <numeric>

INITIALIZE_EASYLOGGINGPP

using std::string;
using std::vector;

static void usage() {
  printf("Usage:\n");
  printf("  compute_parents divide_tree.dvt output_file\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -m min_prominence Minimum prominence threshold for output, default = 100\n");
  exit(1);
}

// Return string representing complete command line
static string argsToString(int argc, char **argv) {
  string args = 
    std::accumulate(argv + 1, argv + argc, string(argv[0]),  
      [](auto&& lhs, auto&& rhs) { 
        return std::forward<decltype(lhs)>(lhs) + ' ' + rhs; 
      }); 
  return args;
}

int main(int argc, char **argv) {
  Elevation minProminence = 100;

  auto commandLine = argsToString(argc, argv);

  // Parse options
  START_EASYLOGGINGPP(argc, argv);

  int ch;
  // Swallow --v that's parsed by the easylogging library
  const struct option long_options[] = {
    {"v", required_argument, nullptr, 0},
    {nullptr, 0, 0, 0},
  };
  while ((ch = getopt_long(argc, argv, "m:", long_options, nullptr)) != -1) {
    switch (ch) {
    case 'm':
      minProminence = static_cast<Elevation>(atof(optarg));
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
  islandTree->build(false);  // TODO: Flag for bathymetry?

  // Build line parent tree
  LineTree *lineTree = new LineTree(*divideTree);
  lineTree->build();

  FILE *file = fopen(outputFilename.c_str(), "wb");
  fprintf(file, "# Prominence and line parents generated at %s\n", getTimeString().c_str());
  fprintf(file, "Command line: %s\n", commandLine.c_str());
  const CoordinateSystem &coords = divideTree->coordinateSystem();
  const vector<DivideTree::Node> &divideNodes = divideTree->nodes();
  const vector<IslandTree::Node> &islandNodes = islandTree->nodes();
  const vector<LineTree::Node> &lineNodes = lineTree->nodes();
  const vector<Peak> &peaks = divideTree->peaks();
  const vector<Saddle> &saddles = divideTree->saddles();
  for (int i = 1; i < (int) divideNodes.size(); ++i) {
    auto prom = islandNodes[i].prominence;
    // Skip dinky peaks
    if (prom < minProminence) {
      continue;
    }

    auto &childPeak = peaks[i - 1];
    LatLng childPos = coords.getLatLng(childPeak.location);
    auto elev = childPeak.elevation;
    int parentId = lineNodes[i].parentId;

    int promParentId = DivideTree::Node::Null;
    int lineParentId = DivideTree::Node::Null;

    VLOG(3) << "Considering peak " <<
        childPos.latitude() << "," << childPos.longitude() << ",P=" << prom;

    // Find saddle
    LatLng colPos(0, 0);
    // Don't look for parents for landmass high points
    if (prom != childPeak.elevation) {
      if (islandNodes[i].keySaddleId != IslandTree::Node::Null) {
        colPos = coords.getLatLng(saddles[islandNodes[i].keySaddleId - 1].location);
      }
    
      // Walk up line tree, looking for first peak with higher prominence.
      // (Prominence values are stored in island tree.)
      while (parentId != DivideTree::Node::Null) {
        auto parentProm = islandNodes[parentId].prominence;
        if (promParentId == DivideTree::Node::Null && parentProm > prom) {
          promParentId = parentId;
        }
        
        auto parentElevation = peaks[parentId - 1].elevation;
        // TODO: Filter to line parent prom >= minProminence
        if (lineParentId == DivideTree::Node::Null && parentElevation >= elev) {
          lineParentId = parentId;
        }
        
        // Found both parents?
        if (lineParentId != DivideTree::Node::Null &&
            promParentId != DivideTree::Node::Null) {
          break;
        }
        
        parentId = lineNodes[parentId].parentId;
      }
    }

    auto promParentLat = 0.0;
    auto promParentLng = 0.0;
    auto promParentProm = 0.0;
    auto lineParentLat = 0.0;
    auto lineParentLng = 0.0;
    auto lineParentElev = 0.0;
    
    if (promParentId == DivideTree::Node::Null) {
      VLOG(2) << "No prominence parent for peak " <<
        childPos.latitude() << "," << childPos.longitude() << ",P=" << prom;
    } else {
      auto &promParentPeak = peaks[promParentId - 1];
      LatLng promParentPos = coords.getLatLng(promParentPeak.location);
      promParentLat = promParentPos.latitude();
      promParentLng = promParentPos.longitude();
      promParentProm = islandNodes[parentId].prominence;
    }
    
    if (lineParentId == DivideTree::Node::Null) {
      VLOG(2) << "No line parent for peak " <<
        childPos.latitude() << "," << childPos.longitude() << ",P=" << prom;
    } else {
      auto &lineParentPeak = peaks[lineParentId - 1];
      LatLng lineParentPos = coords.getLatLng(lineParentPeak.location);
      lineParentLat = lineParentPos.latitude();
      lineParentLng = lineParentPos.longitude();
      lineParentElev = lineParentPeak.elevation;
    }
    
    fprintf(file, "%.4f,%.4f,%.4f,%.4f,%.2f,%.2f,%.4f,%.4f,%.2f,%.4f,%.4f,%.2f\n",
            childPos.latitude(), childPos.longitude(),
            colPos.latitude(), colPos.longitude(),
            elev, prom,
            promParentLat, promParentLng, promParentProm,
            lineParentLat, lineParentLng, lineParentElev);
  }
  
  delete islandTree;
  delete lineTree;
  delete divideTree;
  
  return 0;
}
