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

#include "tile_loading_policy.h"

#include "flt_loader.h"
#include "glo_loader.h"
#include "hgt_loader.h"
#include "tile.h"
#include "easylogging++.h"

using std::string;

BasicTileLoadingPolicy::BasicTileLoadingPolicy(const string &directory, FileFormat format)
    : mDirectory(directory),
      mFileFormat(format),
      mNeighborEdgeLoadingEnabled(false) {
}

void BasicTileLoadingPolicy::enableNeighborEdgeLoading(bool enabled) {
  mNeighborEdgeLoadingEnabled = enabled;
}

Tile *BasicTileLoadingPolicy::loadTile(int minLat, int minLng) const {
  Tile *tile = loadInternal(minLat, minLng);
  if (tile == nullptr) {
    return nullptr;
  }

  // Although some data sets (like NED) are advertised as "seamless",
  // meaning pixels in the overlap areas are identical, in practice
  // this isn't so.  Small differences in elevation in the overlap
  // pixels can cause big problems in prominence calculations, like
  // peaks or saddles detected on one side of the border, but not the
  // other.  We really need the overlap pixels to be identical.
  //
  // Despite the major performance penalty, we force the pixels to be
  // identical by loading our right and bottom neighbors and copying
  // their leftmost column and topmost row.  Note that this still
  // leaves ambiguity in our bottom right pixel.  That would be even
  // more expensive to fix, so we leave it alone in the hopes that
  // fixing it isn't necessary.
  //
  if (mNeighborEdgeLoadingEnabled) {
    switch (mFileFormat) {
    case FileFormat::HGT:  // Fall through
    case FileFormat::NED13_ZIP:
    case FileFormat::NED1_ZIP:
      copyPixelsFromNeighbors(tile, minLat, minLng);
      break;

    case FileFormat::GLO30: {
      // GLO30 "helpfully" removes the last row and column from each tile,
      // so we need to stick them back on.
      Tile *newTile = appendPixelsFromNeighbors(tile, minLat, minLng);
      delete tile;
      tile = newTile;
      break;
    }
      
    default:
      LOG(ERROR) << "Unsupported tile file format";
      return nullptr;
    }
  }

  return tile;
}

Tile *BasicTileLoadingPolicy::loadInternal(int minLat, int minLng) const {
  TileLoader *loader = nullptr;

  switch (mFileFormat) {
  case FileFormat::HGT:
    loader = new HgtLoader();
    break;

  case FileFormat::NED13_ZIP:  // fall through
  case FileFormat::NED1_ZIP:
    loader = new FltLoader(mFileFormat);
    break;
    
  case FileFormat::GLO30: 
    loader = new GloLoader();
    break;    

  default:
    LOG(ERROR) << "Unsupported tile file format";
    return nullptr;
  }
    
  Tile *tile = loader->loadTile(mDirectory, minLat, minLng);
  
  delete loader;
  return tile;
}

Tile *BasicTileLoadingPolicy::copyPixelsFromNeighbors(Tile *tile, int minLat, int minLng) const {
  Tile *neighbor = loadInternal(minLat - 1, minLng);  // bottom neighbor
  if (neighbor != nullptr) {
    for (int i = 0; i < tile->width(); ++i) {
      tile->set(i, tile->height() - 1, neighbor->get(i, 0));
    }
  }
  
  int rightLng = (minLng == 179) ? -180 : (minLng + 1);  // antimeridian
  neighbor = loadInternal(minLat, rightLng);  // right neighbor
  if (neighbor != nullptr) {
    for (int i = 0; i < tile->height(); ++i) {
      tile->set(tile->width() - 1, i, neighbor->get(0, i));
    }
  }
  return tile;
}

Tile *BasicTileLoadingPolicy::appendPixelsFromNeighbors(Tile *tile, int minLat, int minLng) const {
  // First copy over existing samples
  int oldWidth = tile->width();
  int oldHeight = tile->height();
  int newWidth = oldWidth + 1;
  int newHeight = oldHeight + 1;

  Elevation *samples = new Elevation[newWidth * newHeight];

  // Fill in with NODATA in case there aren't neighbors
  for (int i = 0; i < newWidth * newHeight; ++i) {
    samples[i] = Tile::NODATA_ELEVATION;
  }

  // Copy over existing values
  for (int row = 0; row < oldHeight; ++row) {
    int rowStart = row * newWidth;
    for (int col = 0; col < oldWidth; ++col) {
      samples[rowStart + col] = tile->get(col, row);
    }
  }

  int bottomLat = minLat - 1;
  Tile *neighbor = loadInternal(bottomLat, minLng);  // bottom neighbor
  if (neighbor != nullptr) {
    for (int i = 0; i < oldWidth; ++i) {
      samples[oldHeight * newWidth + i] = neighbor->get(i, 0);
    }
  }
  
  int rightLng = (minLng == 179) ? -180 : (minLng + 1);  // antimeridian
  neighbor = loadInternal(minLat, rightLng);  // right neighbor
  if (neighbor != nullptr) {
    for (int i = 0; i < oldHeight; ++i) {
      samples[i * newWidth + oldWidth] = neighbor->get(0, i);
    }
  }

  // Have to get a single corner pixel from the SE neighbor--annoying
  neighbor = loadInternal(bottomLat, rightLng);
  if (neighbor != nullptr) {
    samples[newHeight * newWidth - 1] = neighbor->get(0, 0);
  }
  
  // We've made the tile a little bigger in extents.
  float minLatitude = tile->minLatitude() - 1.0f / tile->height();
  float maxLatitude = tile->maxLatitude();
  float minLongitude = tile->minLongitude();
  float maxLongitude = tile->maxLongitude() + 1.0f / tile->width();
  Tile *newTile = new Tile(newWidth, newHeight, samples,
                           minLatitude,
                           minLongitude,
                           maxLatitude,
                           maxLongitude);
  
  return newTile;
}
