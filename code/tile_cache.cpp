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
#include "coordinate_system.h"
#include "easylogging++.h"

#include <assert.h>

using std::string;

TileCache::TileCache(TileLoadingPolicy *policy, int maxEntries)
    : mCache(maxEntries),
      mLoadingPolicy(policy) {
  // Allow loading policy to use the cache.  This is a circular dependency,
  // but allows for a nice optimization where the cache can store pixels
  // for neighboring tiles that the loading policy needs (for prominence).
  policy->setTileCache(this);
}

TileCache::~TileCache() {
  // Clear cached first rows and columns
  for (auto row : mFirstRows) {
    delete [] row.second;
  }
  for (auto col : mFirstCols) {
    delete [] col.second;
  }
}

Tile *TileCache::getOrLoad(double minLat, double minLng,
                           const CoordinateSystem &coordinateSystem) {
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
  tile = loadWithoutCaching(minLat, minLng, coordinateSystem);
  
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

Tile *TileCache::loadWithoutCaching(double minLat, double minLng,
                                    const CoordinateSystem &coordinateSystem) {
  Tile *tile = mLoadingPolicy->loadTile(minLat, minLng);
  if (tile == nullptr) {
    return nullptr;
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
          LatLng latlng = coordinateSystem.getLatLng(Offsets(x, y));
          LOG(INFO) << "Removed possible spike at " << latlng.latitude() << ", " << latlng.longitude();
        }
      }
    }
  }

  // As an optimization, remember the first row and column
  auto key = makeCacheKey(minLat, minLng);
  Elevation *firstRow = new Elevation[tile->width()];
  Elevation *firstCol = new Elevation[tile->height()];
  for (auto i = 0; i < tile->width(); ++i) {
    firstRow[i] = tile->get(i, 0);
  }
  for (auto i = 0; i < tile->height(); ++i) {
    firstCol[i] = tile->get(0, i);
  }
  mFirstRows[key] = firstRow;
  mFirstCols[key] = firstCol;
  
  VLOG(1) << "Loaded tile at " << minLat << " " << minLng << " with max elevation " << tile->maxElevation();

  return tile;
}

bool TileCache::getMaxElevation(double lat, double lng, Elevation *elev) {
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

bool TileCache::getFirstRow(double lat, double lng, Elevation **elevs) {
  return getFirstRowOrCol(lat, lng, mFirstRows, elevs);
}

bool TileCache::getFirstColumn(double lat, double lng, Elevation **elevs) {
  return getFirstRowOrCol(lat, lng, mFirstCols, elevs);
}

bool TileCache::getFirstRowOrCol(double lat, double lng, const CachedElevations &cache, Elevation **elevs) {
  assert(elevs != nullptr);

  bool retval = true;
  int key = makeCacheKey(lat, lng);

  mLock.lock();
  auto it = cache.find(key);
  if (it == cache.end()) {
    retval = false;
  } else {
    *elevs = it->second;
  }
  mLock.unlock();

  return retval;
}

int TileCache::makeCacheKey(double minLat, double minLng) const {
  // Round to some reasonable precision
  int latKey = static_cast<int>(minLat * 100);
  int lngKey = static_cast<int>(minLng * 100);
  return latKey * 100000 + lngKey;
}
