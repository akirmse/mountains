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


#ifndef _TILE_CACHE_H_
#define _TILE_CACHE_H_

#include "lock.h"
#include "lrucache.h"
#include "tile.h"
#include "tile_loading_policy.h"

#include <string>
#include <unordered_map>

class TileCache {
public:
  // policy determines how to load a tile.
  // maxEntries is the size of the cache.
  explicit TileCache(TileLoadingPolicy *policy, int maxEntries);

  ~TileCache();

  // Retrieve the tile with the given minimum lat/lng, loading it from disk if necessary
  Tile *getOrLoad(double minLat, double minLng, const CoordinateSystem &coordinateSystem);

  // Load the tile from disk without caching it
  Tile *loadWithoutCaching(double minLat, double minLng,
                           const CoordinateSystem &coordinateSystem);

  // If we've ever loaded the tile with the given minimum lat/lng, set elev to its maximum
  // elevation and return true, otherwise return false.
  bool getMaxElevation(double lat, double lng, Elevation *elev);

  // If we've ever loaded the tile with the given minimum lat/lng, set elevs to its
  // first row and return true, otherwise return false.
  bool getFirstRow(double lat, double lng, Elevation **elevs);

  // If we've ever loaded the tile with the given minimum lat/lng, set elevs to its
  // first column and return true, otherwise return false.
  bool getFirstColumn(double lat, double lng, Elevation **elevs);

private:

  Lock mLock;
  lru_cache<int, Tile *> mCache;
  TileLoadingPolicy *mLoadingPolicy;
  // Map of encoded lat/lng to max elevation in that tile
  std::unordered_map<int, Elevation> mMaxElevations;

  // Map of encoded lat/lng to first row/col in that tile.
  // This is an optimization for the prominence algorithm.
  typedef std::unordered_map<int, Elevation *> CachedElevations;
  CachedElevations mFirstRows;
  CachedElevations mFirstCols;

  Tile *loadInternal(double minLat, double minLng) const;
  
  int makeCacheKey(double minLat, double minLng) const;

  bool getFirstRowOrCol(double lat, double lng, const CachedElevations &cache, Elevation **elevs);
};

#endif  // _TILE_CACHE_H_
