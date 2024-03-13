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


#ifndef _ISOLATION_RESULTS_H_
#define _ISOLATION_RESULTS_H_

// Stores isolation results for a region, and serializes them.

#include "latlng.h"
#include "primitives.h"
#include <string>
#include <vector>

class IsolationResults {
public:
  IsolationResults();

  void addResult(const LatLng &peakLocation, Elevation elevationMeters,
                 const LatLng &higherLocation, double isolationKm);

  bool save(const std::string &directory, double lat, double lng) const;

  static IsolationResults *loadFromFile(const std::string &directory, double lat, double lng);

private:
  struct IsolationResult {
    LatLng peak;
    LatLng higher;
    Elevation peakElevation;
    double isolationKm;    
  };
  
  std::vector<IsolationResult> mResults;
  
  static std::string filenameForCoordinates(double lat, double lng);
};

#endif  // _ISOLATION_RESULTS_H_
