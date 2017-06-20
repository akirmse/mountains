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

#include "peakbagger_point.h"

PeakbaggerPoint::PeakbaggerPoint(int id, const std::string &name,
                                 float latitude, float longitude,
                                 int elevation,
                                 int prominence,
                                 float isolation,
                                 const std::string &state,
                                 int loj_id)
      : Point(latitude, longitude),
        mName(name),
        mState(state),
        mId(id),
        mLojId(loj_id),
        mElevation(elevation),
        mProminence(prominence),
        mIsolation(isolation) {
}

PeakbaggerPoint::~PeakbaggerPoint() {
}

void PeakbaggerPoint::operator=(const PeakbaggerPoint &that) {
  Point::operator=(that);
  mName = that.mName;
  mState = that.mState;
  mId = that.mId;
  mLojId = that.mLojId;
  mElevation = that.mElevation;
  mProminence = that.mProminence;
  mIsolation = that.mIsolation;
}

const std::string &PeakbaggerPoint::name() const {
   return mName;
}

const std::string &PeakbaggerPoint::state() const {
   return mState;
}

int PeakbaggerPoint::id() const {
   return mId;
}

int PeakbaggerPoint::loj_id() const {
   return mLojId;
}

int PeakbaggerPoint::elevation() const {
   return mElevation;
}

bool PeakbaggerPoint::has_prominence() const {
  return mProminence != MISSING_PROMINENCE;
}

int PeakbaggerPoint::prominence() const {
  return mProminence;
}

float PeakbaggerPoint::isolation() const {
  return mIsolation;
}
