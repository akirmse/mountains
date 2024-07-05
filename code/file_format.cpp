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

#include "file_format.h"
#include "degree_coordinate_system.h"
#include "utm_coordinate_system.h"
#include "easylogging++.h"

#include <cassert>
#include <cmath>
#include <map>

using std::string;

int FileFormat::rawSamplesAcross() const {
  switch (mValue) {
  case Value::NED13:  // fall through
  case Value::NED13_ZIP:
    return 10812;
  case Value::NED1_ZIP:   return 3612;
  case Value::NED19:      return 8112;
  case Value::HGT:        return 1201;
  case Value::HGT30:      return 3601;
  case Value::THREEDEP_1M:  return 10012;
  case Value::LIDAR: return 10000;
  default:
    // In particular, fail on GLO, because this number is variable with latitude.
    LOG(ERROR) << "Couldn't compute tile size of unknown file format";
    exit(1);
  }
}

int FileFormat::inMemorySamplesAcross() const {
  switch (mValue) {
  case Value::NED13:  // fall through
  case Value::NED13_ZIP:
    return 10801;
  case Value::NED1_ZIP:   return 3601;
  case Value::NED19:      return 8101;
  case Value::HGT: // fall through
  case Value::HGT30:
    return rawSamplesAcross();
  case Value::THREEDEP_1M:  return 10001;
  case Value::GLO30: // Fall through
  case Value::FABDEM:
    return 3601;
  case Value::LIDAR: return 10000;
  default:
    LOG(ERROR) << "Couldn't compute tile size of unknown file format";
    exit(1);
  }
}


double FileFormat::degreesAcross() const {
  switch (mValue) {
  case Value::NED13:  // fall through
  case Value::NED13_ZIP:
    return 1.0;
  case Value::NED1_ZIP:   return 1.0;
  case Value::NED19:      return 0.25;
  case Value::HGT:    // fall through
  case Value::HGT30:
    return 1.0;
  case Value::GLO30:  // Fall through
  case Value::FABDEM:
    return 1.0;
  case Value::LIDAR: return 0.1;
  case Value::THREEDEP_1M:
    // This is a misnomer, as these tiles are in UTM coordinates.  The "degrees" across
    // means one x or y unit per tile (where each tile is 10000m in UTM).
    return 1.0;
  default:
    LOG(ERROR) << "Couldn't compute degree span of unknown file format";
    exit(1);
  }
}

bool FileFormat::isUtm() const {
  return mValue == Value::THREEDEP_1M;
}

CoordinateSystem *FileFormat::coordinateSystemForOrigin(double lat, double lng, int utmZone) const {
  switch (mValue) {
  case Value::NED13_ZIP:  // fall through
  case Value::NED13:
  case Value::NED1_ZIP:   
  case Value::NED19:      
  case Value::HGT:
  case Value::HGT30:
  case Value::GLO30:
  case Value::FABDEM: {
    // The -1 removes overlap with neighbors
    int samplesPerDegreeLat = static_cast<int>((inMemorySamplesAcross() - 1) / degreesAcross());
    int samplesPerDegreeLng = static_cast<int>((inMemorySamplesAcross() - 1) / degreesAcross());
    return new DegreeCoordinateSystem(lat, lng,
                                      lat + degreesAcross(),
                                      lng + degreesAcross(),
                                      samplesPerDegreeLat, samplesPerDegreeLng);
  }

  case Value::LIDAR: {
    int samplesPerDegreeLat = static_cast<int>(std::round(inMemorySamplesAcross() / degreesAcross()));
    int samplesPerDegreeLng = static_cast<int>(std::round(inMemorySamplesAcross() / degreesAcross()));
    return new DegreeCoordinateSystem(lat, lng,
                                      lat + degreesAcross(),
                                      lng + degreesAcross(),
                                      samplesPerDegreeLat, samplesPerDegreeLng);
  }
    

  case Value::THREEDEP_1M:
    // 10km x 10km tiles, NW corner
    return new UtmCoordinateSystem(utmZone,
                                   static_cast<int>(lng * 10000),
                                   static_cast<int>((lat - 1) * 10000),
                                   static_cast<int>((lng + 1) * 10000),
                                   static_cast<int>(lat * 10000),
                                   1.0);
    
  default:
    assert(false);
  }

  return nullptr;
}

FileFormat *FileFormat::fromName(const string &name) {
  const std::map<string, FileFormat> fileFormatNames = {
    { "SRTM",      Value::HGT, },
    { "SRTM30",    Value::HGT30, },
    { "NED1-ZIP",  Value::NED1_ZIP, },
    { "NED13",     Value::NED13, },
    { "NED13-ZIP", Value::NED13_ZIP, },
    { "NED19",     Value::NED19, },
    { "GLO30",     Value::GLO30, },
    { "FABDEM",    Value::FABDEM, },
    { "3DEP-1M",   Value::THREEDEP_1M, },
    { "LIDAR",     Value::LIDAR, },
  };

  auto it = fileFormatNames.find(name);
  if (it == fileFormatNames.end()) {
    return nullptr;
  }

  return new FileFormat(it->second);
}
