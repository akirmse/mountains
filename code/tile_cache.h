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
#include "point_map.h"
#include "tile.h"
#include "tile_loading_policy.h"

#include <string>
#include <unordered_map>

class TileCache {
public:
  // policy determines how to load a tile.
  // externalPeaks is a set of peaks whose elevations are written into tiles as
  // they're loaded; they're "ground truth" elevations.  May be nullptr.
  // maxEntries is the size of the cache.
  explicit TileCache(TileLoadingPolicy *policy, PointMap *externalPeaks, int maxEntries);

  ~TileCache();

  // Retrieve the tile with the given minimum lat/lng, loading it from disk if necessary
  Tile *getOrLoad(int minLat, int minLng);

  // Load the tile from disk without caching it
  Tile *loadWithoutCaching(int minLat, int minLng);

  // If we've ever loaded the tile with the given minimum lat/lng, set elev to its maximum
  // elevation and return true, otherwise return false.
  bool getMaxElevation(int lat, int lng, int *elev);
  
private:

  Lock mLock;
  lru_cache<int, Tile *> mCache;
  TileLoadingPolicy *mLoadingPolicy;
  // Map of encoded lat/lng to max elevation in that tile
  std::unordered_map<int, int> mMaxElevations;
  // External peak elevations, written into tiles as they're loaded
  PointMap *mExternalPeaks;

  Tile *loadInternal(int minLat, int minLng) const;
  
  int makeCacheKey(int minLat, int minLng) const;
};

#endif  // _TILE_CACHE_H_
