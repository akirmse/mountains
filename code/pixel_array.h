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

#ifndef _PIXEL_ARRAY_H_
#define _PIXEL_ARRAY_H_

#include "tile.h"

#include <limits>

// A 2D array of integer values

template<typename Pixel> class PixelArray {
public:
  PixelArray(int width, int height) :
    mWidth(width),
    mHeight(height) {
      // Initialize to empty
      mPixels = new Pixel[mWidth * mHeight]();
  }
  
  ~PixelArray() {
    delete [] mPixels;
  }

  // A non-terrain value; also don't overlap with peak or saddle
  // indexes.
  static const Pixel EmptyPixel = std::numeric_limits<Pixel>::min();  

  Pixel get(int x, int y) const {
    return mPixels[y * mWidth + x];
  }
  
  void set(int x, int y, Pixel value) {
    mPixels[y * mWidth + x] = value;
  }

  // Set count pixels horizontally starting at (x, y) to the given value
  void setRange(int x, int y, Pixel value, int count) {
    Pixel *ptr = mPixels + y * mWidth + x;
    for (int i = 0; i < count; ++i) {
      *ptr++ = value;
    }
  }

  void setAllPixels(Pixel value) {
    setRange(0, 0, value, mWidth * mHeight);
  }

private:
  int mWidth;
  int mHeight;
  Pixel *mPixels;
};

#endif  // _PIXEL_ARRAY_H_
