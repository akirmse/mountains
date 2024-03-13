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

#include "util.h"
#include <ctime>
#include <sstream>
#ifdef PLATFORM_WINDOWS
#include <io.h>
#define R_OK 0
#include <windows.h>
#endif
#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

using std::string;

float feetToMeters(float feet) {
  return feet * 0.3048f;
}

float metersToFeet(float meters) {
  return meters / 0.3048f;
}

double adjustCoordinate(double coordinate) {
  // A tile is not going to be smaller than 0.1 degrees or so, so this
  // amount should be safe.
  const double epsilon = 0.001;
  return coordinate + ((coordinate >= 0) ? epsilon : -epsilon);
}

string trim(const string &s) {
  static const std::string whitespace = " \t\f\v\n\r";
  std::size_t start = s.find_first_not_of(whitespace);
  std::size_t end = s.find_last_not_of(whitespace);

  if (start == string::npos || end == string::npos) {
    return "";
  }
  
  return s.substr(start, (end - start) + 1);
}

std::vector<string> &split(const string &s,
                           char delim,
                           std::vector<string> &elems) {
  elems.clear();
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

string getTempDir() {
#ifdef PLATFORM_LINUX
   return "/tmp";
#elif PLATFORM_WINDOWS
   char path[PATH_MAX];
   GetTempPath(PATH_MAX, path);
   return path;
#endif
}

string getTimeString() {
  time_t rawtime;
  struct tm *timeinfo;
  char buffer[80];

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return buffer;
}

bool fileExists(const std::string &filename) {
  return access(filename.c_str(), R_OK) == 0;
}

