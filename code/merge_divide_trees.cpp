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

// A tool to load and merge multiple divide tree files

#include "divide_tree.h"
#include "island_tree.h"

#include "easylogging++.h"
#include "getopt.h"

INITIALIZE_EASYLOGGINGPP

using std::string;
using std::vector;

static void usage() {
  printf("Usage:\n");
  printf("  merge_divide_trees output_file_prefix input_file [...]\n");
  printf("  Input file should have .dvt extension\n");
  printf("  Output file prefix should have no extension\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -f                Finalize output tree: delete all runoffs and then prune\n");
  printf("  -m min_prominence Minimum prominence threshold for output, default = 300ft\n");
  exit(1);
}

static bool writeStringToOutputFile(const string &base, const string &fname, const string &str) {
  string filename = base + fname;
  FILE *file = fopen(filename.c_str(), "wb");
  if (file == nullptr) {
    LOG(ERROR) << "Couldn't open output file " << filename;
    return false;
  }
  fprintf(file, "%s", str.c_str());
  fclose(file);
  return true;
}

static bool mergeTrees(DivideTree *tree1, DivideTree *tree2) {
  // Put both trees in same coordinate system, keeping coordinates positive
  const CoordinateSystem &coords1 = tree1->coordinateSystem();
  const CoordinateSystem &coords2 = tree2->coordinateSystem();

  auto newCoords = coords1.mergeWith(coords2);

  if (!tree1->setOrigin(*newCoords) || !tree2->setOrigin(*newCoords)) {
    return false;
  }

  tree1->merge(*tree2);
  
  return true;
}

int main(int argc, char **argv) {
  int minProminence = 300;
  bool finalize = false;
  bool flipElevations = false;

  // Parse options
  START_EASYLOGGINGPP(argc, argv);

  int ch;
  string str;
  while ((ch = getopt(argc, argv, "afm:")) != -1) {
    switch (ch) {
    case 'a':
      flipElevations = true;
      break;
      
    case 'f':
      finalize = true;
      break;
      
    case 'm':
      minProminence = atoi(optarg);
      break;
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 2) {
    usage();
  }

  string outputFilename = argv[0];
  DivideTree *divideTree = nullptr;
  for (int arg = 1; arg < argc; ++arg) {
    string inputFilename = argv[arg];
    VLOG(1) << "Loading tree from " << inputFilename;
    
    DivideTree *newTree = DivideTree::readFromFile(inputFilename);
    if (newTree == nullptr) {
      LOG(ERROR) << "Failed to load divide tree from " << inputFilename;
      return 1;
    }

    if (divideTree == nullptr) {
      divideTree = newTree;
    } else {
      mergeTrees(divideTree, newTree);
      delete newTree;
    }

    // Nuke any basin saddles created during merge
    divideTree->compact();
  }

  //
  // Build island tree, compute prominence
  //

  VLOG(1) << "Building prominence island tree";

  IslandTree *unprunedIslandTree = new IslandTree(*divideTree);
  unprunedIslandTree->build();

  if (finalize) {
    divideTree->deleteRunoffs();  // Does not affect island tree
  }
  // TODO: For performance, could prune periodically during a big merge, maybe every N merges
  divideTree->prune(minProminence, *unprunedIslandTree);

  //
  // Write outputs: divide tree, island tree, prominence values
  //
  
  VLOG(1) << "Writing outputs";
  
  // Write .dvt
  if (!divideTree->writeToFile(outputFilename + ".dvt")) {
    LOG(ERROR) << "Failed to write merged divide tree to " << outputFilename;
  }

  // Write KML
  writeStringToOutputFile(outputFilename, "-divide_tree.kml", divideTree->getAsKml());

  delete unprunedIslandTree;
  unprunedIslandTree = nullptr;

  // Build new island tree on pruned divide tree to get final prominence values
  IslandTree *prunedIslandTree = new IslandTree(*divideTree);
  prunedIslandTree->build();
  
  // Write final prominence value table
  string filename = outputFilename + ".txt";
  FILE *file = fopen(filename.c_str(), "wb");
  const CoordinateSystem &coords = divideTree->coordinateSystem();
  const vector<IslandTree::Node> &nodes = prunedIslandTree->nodes();
  const vector<Peak> &peaks = divideTree->peaks();
  const vector<Saddle> &saddles = divideTree->saddles();
  for (int i = 1; i < (int) nodes.size(); ++i) {
    if (nodes[i].prominence >= minProminence) {
      const Peak &peak = peaks[i - 1];
      LatLng peakpos = coords.getLatLng(peak.location);
      LatLng colpos(0, 0);
      if (nodes[i].keySaddleId != IslandTree::Node::Null) {
        colpos = coords.getLatLng(saddles[nodes[i].keySaddleId - 1].location);
      }

      // Flip elevations (if computing anti-prominence)
      int elevation = peak.elevation;
      if (flipElevations) {
        elevation = -elevation;
      }

      fprintf(file, "%.4f,%.4f,%d,%.4f,%.4f,%d\n",
              peakpos.latitude(), peakpos.longitude(), elevation,
              colpos.latitude(), colpos.longitude(),
              nodes[i].prominence);
    }
  }
  fclose(file);

  delete divideTree;
  delete prunedIslandTree;
  
  return 0;
}
