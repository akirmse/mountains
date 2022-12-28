
#
# Stores one or more polygonal regions and can determine if a point is inside.
#
# In general this does not support wrapping around the antimeridian.  However,
# the specific case of the polygon extending a short way east of the antimeridian
# can be handled by calling wrapAntimeridian(), and then testing points
# whose longitudes have been adjusted to > 180 degrees.
#

from typing import List

import os

class Filter:

    # Polygons are stored as an array of [lng, lat] -- note the order
    
    def __init__(self):
        self.polygons = []
    
    def addPolygonsFromKml(self, filename: str) -> bool:
        if not os.path.exists(filename):
            print(f"Can't find polygon file {filename}")
            return False

        inCoordinates = False
        polygon = []
        with open(filename, "r") as f:
            for line in f:
                if "<coordinates>" in line:
                    inCoordinates = True
                    line = line.split("<coordinates>", 1)[1]

                endFound = False
                if inCoordinates:
                    # Look for </coordinates> on same line
                    if "</coordinates>" in line:
                        line = line.split("</coordinates>", 1)[0]
                        endFound = True

                    for field in line.split(" "):
                        elements = field.split(",")
                        if len(elements) >= 2:
                            lat = float(elements[1])  # Note order in KML
                            lng = float(elements[0])
                            polygon.append([lng, lat])
                    
                if endFound or "</coordinates>" in line:
                    inCoordinates = False
                    self.polygons.append(polygon)
                    polygon = []
            
        
        numPoints = 0
        for polygon in self.polygons:
            numPoints += len(polygon)

        print(f"Read {len(self.polygons)} polygons with {numPoints} points")
        
        return True

    # Point is [lat, lng]
    def isPointInside(self, point: List[float]) -> bool:
        if len(self.polygons) == 0:
            return True

        for polygon in self.polygons:
            inside = False
            testx = point[1]
            testy = point[0]
            
            # Horizontal ray cast, check parity of # of polygon intersections.
            # See http://stackoverflow.com/questions/11716268/point-in-polygon-algorithm
            i = 0
            j = len(polygon) - 1
            while i < len(polygon):
                if ((polygon[i][1] > testy) != (polygon[j][1] > testy)) and \
                (testx < (polygon[j][0] - polygon[i][0]) * 
                 (testy - polygon[i][1]) / (polygon[j][1] - polygon[i][1]) + polygon[i][0]):
                     inside = not inside
                j = i
                i += 1
            
            if inside:
                return True
                     
        return False

    def intersects(self, minLat: float, maxLat: float, minLng: float, maxLng: float) -> bool:
        """Does given rectangle intersect any of our polygons?"""
        p1 = [minLat, minLng]
        p2 = [minLat, maxLng]
        p3 = [maxLat, maxLng]
        p4 = [maxLat, minLng]
        points = [p1, p2, p3, p4]
        # If any corner of the rectangle is inside, there is an intersection
        for point in points:
            if self.isPointInside(point):
                return True
        
        for polygon in self.polygons:
            # Try intersecting the four sides of the rectangle with all edges of the polygon
            for i, _ in enumerate(points):
                point1 = points[i]
                point2 = points[(i + 1) % len(points)]
                j = 0
                k = len(polygon) - 2  # last point == first point; skip it
                while j < len(polygon):
                    if self._segmentsIntersect(point1[1], point1[0],
                                               point2[1], point2[0],
                                               polygon[j][0], polygon[j][1],
                                               polygon[k][0], polygon[k][1]):
                        return True
                    k = j
                    j = j + 1
                    

        # Is any point of polygon inside the rectangle?
        for point in polygon:
            if point[1] >= minLat and point[1] <= maxLat and \
               point[0] >= minLng and point[0] <= maxLng:
                return True

        return False

    def _segmentsIntersect(self, p0x: float, p0y: float, p1x: float, p1y: float,
                           p2x: float, p2y: float, p3x: float, p3y: float) -> bool:
        """Returns true if line segment p0-p1 intersects with segment p2-p3."""
        # See http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
        s1_x = p1x - p0x
        s1_y = p1y - p0y
        s2_x = p3x - p2x
        s2_y = p3y - p2y

        s = (-s1_y * (p0x - p2x) + s1_x * (p0y - p2y)) / (-s2_x * s1_y + s1_x * s2_y)
        t = ( s2_x * (p0y - p2y) - s2_y * (p0x - p2x)) / (-s2_x * s1_y + s1_x * s2_y)

        if (s >= 0 and s <= 1 and t >= 0 and t <= 1):
            return True

        return False
