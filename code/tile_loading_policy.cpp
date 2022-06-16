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
  }

  return tile;
}

Tile *BasicTileLoadingPolicy::loadInternal(int minLat, int minLng) const {
  Tile *tile = nullptr;

  switch (mFileFormat) {
  case FileFormat::HGT: {
    HgtLoader loader;
    tile = loader.loadTile(mDirectory, minLat, minLng);
    break;
  }

  case FileFormat::NED13_ZIP:  // fall through
  case FileFormat::NED1_ZIP: {
    FltLoader loader(mFileFormat);
    tile = loader.loadTile(mDirectory, minLat, minLng);
    break;
  }

  default:
    LOG(ERROR) << "Unsupported tile file format";
    break;
  }

  return tile;
}
