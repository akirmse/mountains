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

#include "kml_writer.h"

using std::string;

KMLWriter::KMLWriter(const CoordinateSystem &coords)
    : mCoords(coords) {
  mKml = "<kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>\n";

  mKml += "<Style id=\"peak\"><IconStyle><Icon><href>http://maps.google.com/mapfiles/kml/shapes/volcano.png</href></Icon></IconStyle></Style>\n";
  mKml += "<Style id=\"saddle\"><IconStyle><Icon><href>http://maps.google.com/mapfiles/kml/shapes/homegardenbusiness.png</href></Icon></IconStyle></Style>\n";
  mKml += "<Style id=\"basinsaddle\"><IconStyle><Icon><href>http://maps.google.com/mapfiles/kml/shapes/placemark_circle.png</href></Icon></IconStyle></Style>\n";

  mKml += "<Style id=\"runoff\"><IconStyle><Icon><href>http://maps.google.com/mapfiles/kml/shapes/info_circle.png</href></Icon></IconStyle>\n";
  mKml += "<LineStyle><color>ff800000</color></LineStyle></Style>\n";
}

void KMLWriter::startFolder(const std::string &name) {
  mKml += "<Folder><name>" + name + "</name>";
}

void KMLWriter::endFolder() {
  mKml += "</Folder>";
}
  
void KMLWriter::addPeak(const Peak &peak, const std::string &name) {
  LatLng peakpos = mCoords.getLatLng(peak.location);
  mKml += "<Placemark><styleUrl>#peak</styleUrl><Point><coordinates>\n";
  mKml += std::to_string(peakpos.longitude());
  mKml += ",";
  mKml += std::to_string(peakpos.latitude());
  mKml += ",";
  mKml += std::to_string(peak.elevation);
  mKml += "\n";
  mKml += "</coordinates></Point>";
  mKml += "<name>";
  mKml += name;
  mKml += "</name>";
  mKml += "</Placemark>\n";
}

void KMLWriter::addRunoff(const Runoff &runoff, const std::string &name) {
  LatLng pos = mCoords.getLatLng(runoff.location);
  mKml += "<Placemark><styleUrl>#runoff</styleUrl><Point><coordinates>\n";
  mKml += std::to_string(pos.longitude());
  mKml += ",";
  mKml += std::to_string(pos.latitude());
  mKml += ",";
  mKml += std::to_string(runoff.elevation);
  mKml += "\n";
  mKml += "</coordinates></Point>";
  mKml += "<name>";
  mKml += name;
  mKml += "</name></Placemark>\n";
}

void KMLWriter::addPromSaddle(const Saddle &saddle, const std::string &name) {
  addSaddle(saddle, "#saddle", name);
}

void KMLWriter::addBasinSaddle(const Saddle &saddle, const std::string &name) {
  addSaddle(saddle, "#basinsaddle", name);
}

void KMLWriter::addGraphEdge(const Peak &peak1, const Peak &peak2, const Saddle &saddle) {
  LatLng peak1pos = mCoords.getLatLng(peak1.location);
  LatLng saddlepos = mCoords.getLatLng(saddle.location);
  LatLng peak2pos = mCoords.getLatLng(peak2.location);
  
  mKml += "<Placemark><LineString><coordinates>\n";
  mKml += std::to_string(peak1pos.longitude());
  mKml += ",";
  mKml += std::to_string(peak1pos.latitude());
  mKml += ",";
  mKml += std::to_string(peak1.elevation);
  mKml += "\n";
  mKml += std::to_string(saddlepos.longitude());
  mKml += ",";
  mKml += std::to_string(saddlepos.latitude());
  mKml += ",";
  mKml += std::to_string(saddle.elevation);
  mKml += "\n";
  mKml += std::to_string(peak2pos.longitude());
  mKml += ",";
  mKml += std::to_string(peak2pos.latitude());
  mKml += ",";
  mKml += std::to_string(peak2.elevation);
  mKml += "\n";
  mKml += "</coordinates></LineString></Placemark>\n";
}

void KMLWriter::addRunoffEdge(const Peak &peak, const Runoff &runoff) {
  LatLng peakpos = mCoords.getLatLng(peak.location);
  LatLng runoffpos = mCoords.getLatLng(runoff.location);
  
  mKml += "<Placemark><styleUrl>#runoff</styleUrl><LineString><coordinates>\n";
  mKml += std::to_string(peakpos.longitude());
  mKml += ",";
  mKml += std::to_string(peakpos.latitude());
  mKml += ",";
  mKml += std::to_string(peak.elevation);
  mKml += "\n";
  mKml += std::to_string(runoffpos.longitude());
  mKml += ",";
  mKml += std::to_string(runoffpos.latitude());
  mKml += ",";
  mKml += std::to_string(runoff.elevation);
  mKml += "\n";
  mKml += "</coordinates></LineString></Placemark>\n";
}

void KMLWriter::addPeakEdge(const Peak &peak1, const Peak &peak2) {
  LatLng peak1pos = mCoords.getLatLng(peak1.location);
  LatLng peak2pos = mCoords.getLatLng(peak2.location);
  
  mKml += "<Placemark><LineString><coordinates>\n";
  mKml += std::to_string(peak1pos.longitude());
  mKml += ",";
  mKml += std::to_string(peak1pos.latitude());
  mKml += ",";
  mKml += std::to_string(peak1.elevation);
  mKml += "\n";
  mKml += std::to_string(peak2pos.longitude());
  mKml += ",";
  mKml += std::to_string(peak2pos.latitude());
  mKml += ",";
  mKml += std::to_string(peak2.elevation);
  mKml += "\n";
  mKml += "</coordinates></LineString></Placemark>\n";
}

string KMLWriter::finish() {
  mKml += "</Document></kml>\n";
  return mKml;
}

void KMLWriter::addSaddle(const Saddle &saddle, const char *styleUrl, const string &name) {
  LatLng saddlepos = mCoords.getLatLng(saddle.location);
  mKml += "<Placemark><styleUrl>";
  mKml += styleUrl;
  mKml += "</styleUrl><Point><coordinates>\n";
  mKml += std::to_string(saddlepos.longitude());
  mKml += ",";
  mKml += std::to_string(saddlepos.latitude());
  mKml += ",";
  mKml += std::to_string(saddle.elevation);
  mKml += "\n";
  mKml += "</coordinates></Point>";
  mKml += "<name>";
  mKml += name;
  mKml += "</name>";
  mKml += "</Placemark>\n";
}
