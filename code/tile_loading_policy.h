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


#ifndef _TILE_LOADING_POLICY_H_
#define _TILE_LOADING_POLICY_H_

#include "file_format.h"
#include "tile.h"

#include <string>

// Responsible for loading a tile given lat/lng.

class TileLoadingPolicy {
public:
  virtual Tile *loadTile(float minLat, float minLng) const = 0;
};


// Tile loading policy that loads a single file format from
// a single directory.

class BasicTileLoadingPolicy : public TileLoadingPolicy {
public:
  BasicTileLoadingPolicy(const std::string &directory, const FileFormat &fileFormat);

  // Prominence calculations require that pixels along the edges of
  // tiles are exactly identical.  To enforce this, it turns out to be
  // necessary to physically copy the pixels from neighbors, which
  // makes loading a tile much slower.
  //
  // This is disabled by default.
  void enableNeighborEdgeLoading(bool enabled);

  virtual Tile *loadTile(float minLat, float minLng) const;

private:
  std::string mDirectory;  // Directory for loading tiles
  FileFormat mFileFormat;  
  bool mNeighborEdgeLoadingEnabled;

  // Load tile without modifications
  Tile *loadInternal(float minLat, float minLng) const;

  // Copy pixels from south and east neighbors into this tile, and return it.
  Tile *copyPixelsFromNeighbors(Tile *tile, float minLat, float minLng) const;

  // Create a new tile by appending the first row and column from south
  // and east neighbors to this tile, and return the new tile.
  Tile *appendPixelsFromNeighbors(Tile *tile, float minLat, float minLng) const;
};

#endif  // _TILE_LOADING_POLICY_H_
