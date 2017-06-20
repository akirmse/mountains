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


#ifndef _PEAKBAGGER_POINT_H__
#define _PEAKBAGGER_POINT_H__

#include <string>
#include "point.h"

// Point from the Peakbagger database
class PeakbaggerPoint : public Point {
public:
   static const int MISSING_PROMINENCE = -1;
  
   PeakbaggerPoint(int id, const std::string &name,
                   float latitude, float longitude,
                   int elevation,
                   int prominence,
                   float isolation,
                   const std::string &state,
                   int loj_id);
   PeakbaggerPoint(const PeakbaggerPoint &that)
       : Point(that) {
     *this = that;
   }
   virtual ~PeakbaggerPoint();

   virtual void operator=(const PeakbaggerPoint &that);
  
   const std::string &name() const;
   const std::string &state() const;
   int id() const;
   int loj_id() const;
   int elevation() const;
   bool has_prominence() const;
   int prominence() const;  // only valid if has_prominence() is true
   float isolation() const;
   
private:
   std::string mName;
   std::string mState;
   int mId;
   int mLojId;
   int mElevation;
   int mProminence;
   float mIsolation;
};

#endif  // ifndef _PEAKBAGGER_POINT_H__
