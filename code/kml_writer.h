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

#ifndef _KML_WRITER_H_
#define _KML_WRITER_H_

// Utilities for building up KML text

#include "primitives.h"
#include "coordinate_system.h"

#include <string>

class KMLWriter {
public:

  // Coordinate system is used to convert offsets to lat/lngs
  explicit KMLWriter(const CoordinateSystem &coords);

  void startFolder(const std::string &name);
  void endFolder();
  
  void addPeak(const Peak &peak, const std::string &name);
  void addRunoff(const Runoff &runoff, const std::string &name);
  void addPromSaddle(const Saddle &saddle, const std::string &name);
  void addBasinSaddle(const Saddle &saddle, const std::string &name);

  void addGraphEdge(const Peak &peak1, const Peak &peak2, const Saddle &saddle);
  void addRunoffEdge(const Peak &peak, const Runoff &runoff);
  void addPeakEdge(const Peak &peak1, const Peak &peak2);  

  // End document and return KML
  std::string finish();
  
private:
  const CoordinateSystem &mCoords;
  std::string mKml;

  void addSaddle(const Saddle &saddle, const char *styleUrl, const std::string &name);
};

#endif  // ifndef _KML_WRITER_H_
