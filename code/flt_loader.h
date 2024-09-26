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

// Load a .flt tile. This is the format used some USGS data. Other data
// would need to be converted to .flt externally (e.g. with gdal_translate).

class FltLoader : public TileLoader {
public:
  // The UTM zone is used when loading UTM-based tiles.  In such cases,
  // the latitude will be treated as northing, and the longitude as easting.
  explicit FltLoader(const FileFormat &format, int utmZone);
  
  virtual Tile *loadTile(const std::string &directory, double minLat, double minLng);

private:
  const FileFormat &mFormat;
  int mUtmZone;  // For data in UTM coordinates
  
  Tile *loadFromNEDZipFileInternal(const std::string &directory, double minLat, double minLng);

  Tile *loadFromFltFile(const std::string &directory, double minLat, double minLng);
  
  // Return the filename for the .flt file for the given coordinates
  std::string getFltFilename(double minLat, double minLng, const FileFormat &format);

  // Return hundredths of a degree from the given value
  int fractionalDegree(double degree) const;
};

#endif  // _FLOT_LOADER_H_
