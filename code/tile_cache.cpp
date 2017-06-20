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


#include "tile_cache.h"
#include "peakbagger_point.h"
#include "easylogging++.h"

#include <assert.h>

using std::string;

TileCache::TileCache(const string &directory, PointMap *externalPeaks, int maxEntries)
    : mCache(maxEntries),
      mDirectory(directory),
      mExternalPeaks(externalPeaks),
      mFileFormat(FileFormat::HGT),
      mNeighborEdgeLoadingEnabled(false) {
}

TileCache::~TileCache() {
}

void TileCache::setFileFormat(FileFormat format) {
  mFileFormat = format;
}

void TileCache::enableNeighborEdgeLoading(bool enabled) {
  mNeighborEdgeLoadingEnabled = enabled;
}

Tile *TileCache::getOrLoad(int minLat, int minLng) {
  // In cache?
  int key = makeCacheKey(minLat, minLng);
  Tile *tile = nullptr;
  
  mLock.lock();
  if (mCache.exists(key)) {
    tile = mCache.get(key);
  }
  mLock.unlock();

  if (tile != nullptr) {
    return tile;
  }

  // Not in cache; load it
  tile = loadWithoutCaching(minLat, minLng);
  
  // Add to cache
  mLock.lock();
  if (tile == nullptr) {
    // No terrain => max elevation 0
    mMaxElevations[key] = 0;
  } else {
    mCache.put(key, tile);

    mMaxElevations[key] = tile->maxElevation();
  }
  mLock.unlock();

  return tile;
}

Tile *TileCache::loadWithoutCaching(int minLat, int minLng) {
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

  // Look for big spikes, and replace them with NODATA.  We want to do
  // this before applying external peaks, because external peaks may
  // look suspiciously like spikes, but actually be valid (because the
  // true peak height may stand out quite a bit from the terrain in
  // the tile).
  static const Elevation MAX_LEGAL_ELEVATION_DIFF = 1000;
  for (int y = 0; y < tile->height(); ++y) {
    for (int x = 0; x < tile->width(); ++x) {
      Elevation elev = tile->get(x, y);
      if (elev != Tile::NODATA_ELEVATION) {
        // Enough to check the 4-neighbors.  Kill the higher point.
        if (tile->isInExtents(Offsets(x + 1, y))) {
          Elevation neighborElev = tile->get(x + 1, y);
          if (neighborElev != Tile::NODATA_ELEVATION && elev - neighborElev > MAX_LEGAL_ELEVATION_DIFF) {
            tile->set(x, y, Tile::NODATA_ELEVATION);
          }
        }
        if (tile->isInExtents(Offsets(x, y + 1))) {
          Elevation neighborElev = tile->get(x, y + 1);
          if (neighborElev != Tile::NODATA_ELEVATION && elev - neighborElev > MAX_LEGAL_ELEVATION_DIFF) {
            tile->set(x, y, Tile::NODATA_ELEVATION);
          }
        }
        if (tile->isInExtents(Offsets(x - 1, y))) {
          Elevation neighborElev = tile->get(x - 1, y);
          if (neighborElev != Tile::NODATA_ELEVATION && elev - neighborElev > MAX_LEGAL_ELEVATION_DIFF) {
            tile->set(x, y, Tile::NODATA_ELEVATION);
          }
        }
        if (tile->isInExtents(Offsets(x, y - 1))) {
          Elevation neighborElev = tile->get(x, y - 1);
          if (neighborElev != Tile::NODATA_ELEVATION && elev - neighborElev > MAX_LEGAL_ELEVATION_DIFF) {
            tile->set(x, y, Tile::NODATA_ELEVATION);
          }
        }

        // Print out spikes
        if (tile->get(x, y) == Tile::NODATA_ELEVATION && VLOG_IS_ON(1)) {
          LatLng latlng = tile->latlng(Offsets(x, y));
          LOG(INFO) << "Removed possible spike at " << latlng.latitude() << ", " << latlng.longitude();
        }
      }
    }
  }
  
  // Apply peak elevations, if any.  This forces us to use external (e.g. Peakbagger's)
  // peak elevations, which may differ from those in the raster data.  For some peaks a
  // small change in elevation can make a radical difference in isolation.
  if (mExternalPeaks != nullptr) {
    // Tiles overlap along the edges, and samples extend half a pixel into each
    // neighboring tile.  Thus we need to check all 8 neighbors for peaks that
    // overlap us.
    int numPeaksWritten = 0;
    for (int deltaX = -1; deltaX <= 1; deltaX += 1) {
      for (int deltaY = -1; deltaY <= 1; deltaY += 1) {
        int lng = minLng + deltaX;
        int lat = minLat + deltaY;
        PointMap::Bucket *bucket = mExternalPeaks->lookup(lat, lng);
        if (bucket != nullptr) {
          for (auto it : *bucket) {
            // TODO: Not sure that this cast is a good assumption
            PeakbaggerPoint *pb_point = (PeakbaggerPoint *) it;

            if (tile->isInExtents(pb_point->latitude(), pb_point->longitude())) {
              // Peakbagger elevations are in feet
              tile->setLatLng(pb_point->latitude(), pb_point->longitude(),
                              pb_point->elevation());
              numPeaksWritten += 1;
            }
          }
        }
      }
    }
    VLOG(2) << "Wrote " << numPeaksWritten << " external peaks to tile";
    
    // We've changed the max elevation now
    tile->recomputeMaxElevation();
  }

  VLOG(1) << "Loaded tile at " << minLat << " " << minLng << " with max elevation " << tile->maxElevation();

  return tile;
}

bool TileCache::getMaxElevation(int lat, int lng, int *elev) {
  assert(elev != nullptr);

  bool retval = true;
  int key = makeCacheKey(lat, lng);

  mLock.lock();
  auto it = mMaxElevations.find(key);
  if (it == mMaxElevations.end()) {
    retval = false;
  } else {
    *elev = it->second;
  }
  mLock.unlock();

  return retval;
}

Tile *TileCache::loadInternal(int minLat, int minLng) const {
  Tile *tile = nullptr;

  switch (mFileFormat) {
  case FileFormat::HGT:
    tile = Tile::loadFromHgtFile(mDirectory, minLat, minLng);
    break;

  case FileFormat::NED13_ZIP:
    tile = Tile::loadFromNED13ZipFile(mDirectory, minLat, minLng);
    break;

  case FileFormat::NED1_ZIP:
    tile = Tile::loadFromNED1ZipFile(mDirectory, minLat, minLng);
    break;

  default:
    LOG(ERROR) << "Unsupported tile file format";
    break;
  }

  return tile;
}

int TileCache::makeCacheKey(int minLat, int minLng) const {
  return minLat * 1000 + minLng;
}
