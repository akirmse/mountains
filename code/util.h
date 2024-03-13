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

#ifndef _UTIL_H__
#define _UTIL_H__

#include <string>
#include <vector>
#include <map>
#include <unordered_set>

float feetToMeters(float feet);
float metersToFeet(float meters);

// Trim whitespace from the start and end of the string, return a new string
std::string trim(const std::string &s);

// Adjust the given coordinate by an epsilon value away from 0, so that truncation
// to int doesn't give incorrect values to due floating-point imprecision.
double adjustCoordinate(double coordinate);

/*
 * Split the given string by the given delimiter, putting the
 * result in elems and returning it.
 */

std::vector<std::string> &split(const std::string &s,
                                char delim,
                                std::vector<std::string> &elems);

std::string getTempDir();

// Returns current time and time like "2017-04-14 16:26:15"
std::string getTimeString();

// Returns true if file exists and is readable.
bool fileExists(const std::string &filename);

// Remove the first instance of any (key, value) mapping from the given multimap
template <typename K, typename V>
void removeFromMultimap(std::multimap<K, V> *mmap, K key, V value) {
  const auto range = mmap->equal_range(key);
  auto it = range.first;
  for (; it != range.second; ++it) {
    if (it->second == value) {
      mmap->erase(it);
      break;
    }
  }
}

// Remove all elements of vec whose indices appear in the given set.
template <typename T>
void removeVectorElementsByIndices(std::vector<T> *vec, const std::unordered_set<int> &indices) {
  int to = 0;
  for (int from = 0; from < (int) vec->size(); ++from) {
    if (indices.find(from) == indices.end()) {
      if (from != to) {
        (*vec)[to] = (*vec)[from];
      }
      to += 1;
    }
  }

  vec->erase(vec->begin() + to, vec->end());  // adjust size
}

#endif  // ifndef _UTIL_H__
