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

#ifndef _FLT_LOADER_H_
#define _FLT_LOADER_H_

#include "file_format.h"
#include "tile_loader.h"
#include "tile.h"
#include "util.h"
#include "easylogging++.h"

// Load a .flt tile. This is the format used in National Elevation Dataset (NED) data.

class FltLoader : public TileLoader {
public:
  explicit FltLoader(const FileFormat &format);
  
  virtual Tile *loadTile(const std::string &directory, float minLat, float minLng);

private:
  FileFormat mFormat;
  
  Tile *loadFromNEDZipFileInternal(const std::string &directory, float minLat, float minLng);

  Tile *loadFromFltFile(const std::string &directory, float minLat, float minLng);
  
  // Return the filename for the .flt file for the given coordinates
  std::string getFltFilename(float minLat, float minLng, const FileFormat &format);

  // Return hundredths of a degree from the given value
  int fractionalDegree(float degree) const;
};

#endif  // _FLOT_LOADER_H_
