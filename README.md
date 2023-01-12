# Prominence and isolation

C++ programs for calculating the isolation and prominence from digital
elevation data.

## Building the code

C++14 support is required to build the code.  Binaries are placed in
the "debug" or "release" subdirectories.

### OSX and gcc

Debug version:

```
make  
```

Release version:

```
RELEASE=1 make
```

This has been tested under Mac OS 12.2.1 with clang-1300.0.29.30, and Unbuntu 20.04 with gcc 9.

### Windows

Debug version:

```
nmake -f makefile.win
```

Release version:

```
nmake RELEASE=1 -f makefile.win
```

This has been tested under Windows 10 with Microsoft Visual Studio 2022.

## Running the code

### Source data

Download source DEM data for the region of interest. 90-meter data is
available from NASA (SRTM), or improved void-filled data is at
[viewfinderpanoramas](http://viewfinderpanoramas.org/dem3.html).
Higher resolution data for North America is available at [The National
Map](https://viewer.nationalmap.gov/).

Copernicus 30m GLO30 data can be downloaded from [here](https://registry.opendata.aws/copernicus-dem/). 

Note that SRTM and GLO30 filenames reference the southwest corner of the tile,
while NED filenames reference the northwest corner.

Because tiles need to reference their neighbors when computing prominence, all tiles must reside
in a single directory and have specific names.

#### SRTM

SRTM or the data from Viewfinderpanoramas is delivered as HGT files, with names like this:

```
N59E006.hgt
```

#### NED

The National Elevation Dataset (NED) covers the U.S. at various resolutions:
- NED1 has 1-arcsecond (~90 foot) samples
- NED13 has 1/3-arcsecond (~30 foot) samples
- NED19 has 1/9 arcsecond (~10 foot) samples. 

##### NED1 and NED13

NED data is delivered as zip files, one per square degree, with names like:

```
n59e006.zip
```

The prominence code will extract the FLT file from the zip (using ```7z``` on Windows or ```unzip``` on Mac/Linux).

##### NED19

NED19 data is based on LIDAR.  As of this writing, it covers only part of the U.S.  Data is delivered
as zip files that are 1/4 of a degree on each side.  The zip files containing
the data contain the LIDAR collection name in the filename, making them difficult to discover.  There
is a Python script in the ```scripts``` subdirectory that will discover the tiles for a given lat/lng, 
download them, covert them to FLT format, and run the prominence code on them.

```
usage: run_ned19_prominence.py [-h] --tile_dir TILE_DIR --output_dir
                               OUTPUT_DIR
                               [--prominence_command PROMINENCE_COMMAND]
                               [--threads THREADS]
                               min_lat max_lat min_lng max_lng
```

Multiple LIDAR collections can cover the same tile.  In this case, the script picks one arbitrarily.

#### GLO30

The data is delivered as TIFF files.  Convert them to FLT using ```gdal_translate```, for example:

```
gdal_translate -of EHdr Copernicus_DSM_COG_10_N59_00_E006_00_DEM.tif Copernicus_DSM_COG_10_N59_00_E006_00_DEM.flt 
```

There is a script in the ```scripts``` subdirectory that can automate the downloading and conversion of tiles,
followed by running the prominence program on them.

```
usage: run_glo_prominence.py [-h] [--tile_dir TILE_DIR]
                             [--output_dir OUTPUT_DIR]
                             [--prominence_command PROMINENCE_COMMAND]
                             [--threads THREADS]
                             min_lat max_lat min_lng max_lng
```

#### FABDEM

[FABDEM](https://data.bris.ac.uk/data/dataset/25wfy0f9ukoge2gs7a5mqpq2j7) (Forest And Buildings removed Copernicus DEM) is a version of the GLO30 data that has been modified by machine
learning techniques to remove trees and buildings, leaving the bare earth.  To process FABDEM, use the same ```run_glo_prominence```
Python script as with GLO30, giving the additional argument ```--format FABDEM```.

#### USGS 1m terrain ("3DEP" = 3D Elevation Program)

This LIDAR-based data is high resolution, but has spotty coverage.  This is raw LIDAR data converted
into a regular 1m grid.  It is delivered as TIFF files
in the UTM projection, not lat/lng coordinates. The tiles must be converted to FLT format
before running the prominence program on them.

The data was collected in medium-scale areas, such as a county, and the tiles are organized into
"projects" based on collection.  Thus, it is generally not possible to merge the tiles from multiple
projects with each other.  You must be very careful interpreting the results, as any peaks whose
key cols are outside of the project's coverage will have incorrect prominence.  You can browse
the coverage [here](https://prd-tnm.s3.amazonaws.com/index.html?prefix=StagedProducts/Elevation/1m/Projects/).

There is a script in the ```scripts``` subdirectory that can automate the downloading and conversion of tiles,
followed by running the prominence program on them.

```
usage: run_3dep1m_prominence.py [-h] --tile_dir TILE_DIR --output_dir
                                OUTPUT_DIR --project PROJECT --zone ZONE
                                [--prominence_command PROMINENCE_COMMAND]
                                [--threads THREADS]
                                min_x max_x min_y max_y
```

Note that you must specify the UTM zone (which can be inferred from the filenames of tiles in the project),
the project name (like "CA_SantaClaraCounty_2020_A20"), and the coordinates are specified as X/Y in UTM, with units of 10,000 meters. 
These X and Y coordinates also correspond to the naming of the tiles.

#### Data summary


| Name | Resolution | Coverage | Projection | Download |
-------|------------|----------|------------|----------|
| SRTM   | 90m        | global   | lat/lng |[Link](viewfinderpanoramas.com) |
| NED1   | 30m        | US, Canada, Mexico | lat/lng | [Link](https://prd-tnm.s3.amazonaws.com/index.html?prefix=StagedProducts/Elevation/1/) |
| GLO-30 | 30m        | global minus Azerbaijan and Armenia | lat/lng | [Link](https://registry.opendata.aws/copernicus-dem/) |
| FABDEM | 30m        | GLO-30 minus < -60°S and > 80°N | lat/lng | [Link](https://data.bris.ac.uk/data/dataset/25wfy0f9ukoge2gs7a5mqpq2j7) |
| NED13  | 10m        | US       | lat/lng | [Link](https://prd-tnm.s3.amazonaws.com/index.html?prefix=StagedProducts/Elevation/13)|
| NED19  | 3m         | partial US | lat/lng | [Link](https://prd-tnm.s3.amazonaws.com/index.html?prefix=StagedProducts/Elevation/19) |
| 3DEP1m | 1m         | partial US | UTM | [Link](https://prd-tnm.s3.amazonaws.com/index.html?prefix=StagedProducts/Elevation/1m/Projects/) |

### Isolation

```
isolation -- <min latitude> <max latitude> <min longitude> <max longitude>

Options:
  -i directory     Directory with terrain data
  -m min_isolation Minimum isolation threshold for output, default = 1km
  -o directory     Directory for output data
  -t num_threads   Number of threads, default = 1

```

This will generate one output text file per input tile, containing the
isolation of peaks in that tile.  The files can be merged and sorted
with standard command-line utilities.

The isolation calculation is currently limited to SRTM input data, though
it could fairly easily be extended to the other data sets.

### Prominence

First, generate divide trees for tiles of interest:

```
prominence -- <min latitude> <max latitude> <min longitude> <max longitude>

  Options:
  -i directory      Directory with terrain data
  -o directory      Directory for output data
  -f format         "SRTM", "NED13-ZIP", "NED1-ZIP", "NED19", "3DEP-1M", "GLO30" input files
  -k filename       File with KML polygon to filter input tiles
  -m min_prominence Minimum prominence threshold for output
                    in same units as terrain data, default = 100
  -z                UTM zone (if input data is in UTM)
  -t num_threads    Number of threads, default = 1
```

This will produce divide trees with the .dvt extension, and KML files
that can be viewed in Google Earth.  The unpruned trees are generally
too large to be merged or to load into Earth.  Use the pruned versions
(identified by "pruned" in their filenames).

Next, merge the resultant pruned divide trees into a single, larger divide
tree.  Large merges can be done in parallel with multiple threads. 

```
merge_divide_trees output_file_prefix input_file [...]
  Input file should have .dvt extension
  Output file prefix should have no extension

  Options:
  -f                Finalize output tree: delete all runoffs and then prune
  -m min_prominence Minimum prominence threshold for output, default = 100
  -t num_threads    Number of threads to use, default = 1
```

Specify the "-f" flag to get final output when you no longer need to perform
any further merges.  (In previous versions, merge_divide_tree was serial, and it made sense to 
merge in multiple stages, specifying -f only at the last stage.  Now you
can generally merge everything in one parallel step.)

The output is a dvt file with the merged divide tree, and a text file
with prominence values.  If desired, the text file can be filtered to
exclude peaks outside of a polygon specified in KML, for example, to
restrict the output to a single continent:

```
filter_points input_file polygon_file output_file
  Filters input_file, printing only lines inside the polygon in polygon_file to output_file

  Options:
  -a longitude    Add 360 to any longitudes < this value in polygon.
                  Allows polygon to cross antimeridian (negative longitude gets +360)
```


### Logging

By default, the programs don't print much to the screen.  To see more of what's happening, you can
set the log level on the command line using the flags from the [easylogging](https://github.com/amrayn/easyloggingpp#application-arguments)
library.  Specifying ```--v=1``` will print each major operation, while ```--v=4``` will produce a 
torrent of output.

## Results

The isolation file has one peak per line, in this format:

latitude,longitude,elevation,ILP latitude,ILP longitude,isolation in km

where ILP means isolation limit point.

A zip file with our isolation results for the world is [here](https://drive.google.com/file/d/0B3icWNhBosDXRm1pak56blp1RGc/view?resourcekey=0-MxVs2IKeWLrN-saUoqaweA).

The prominence file has one peak per line, in this format:

latitude,longitude,elevation,key saddle latitude,key saddle longitude,prominence

The units of elevation and prominence are the same as the input terrain data.

A zip file with our prominence results for the world is [here](https://drive.google.com/file/d/1HnMIZemDu3MgyCu7Fdy-VJOsiXTN5sYW/view?usp=share_link), with elevations in meters, down to 100 feet (30.48m) of prominence.  A visualization of the results is [here](http://everymountainintheworld.com).

## Prominence parents and line parents

Given a divide tree, it's possible to compute each peak's *prominence parent*, that is, the first more prominent peak that's encountered
while walking from a peak, then to its key saddle, and then up the ridge up the other side; and its *line parent*, which is the first higher
peak encountered on such a walk.  While prominence parents are parameterless, line parents depend on the prominence threshold chosen to define
a "peak".  In this implementation, the definition of a line parent is implicit in the prominence threshold defined when
building the divide tree (that is, any peak in the divide tree can be a line parent).

```
Usage:
  compute_parents divide_tree.dvt output_file

  Options:
  -m min_prominence Minimum prominence threshold for output, default = 100
```

The input divide tree must be free of runoffs (see the options to ```merge_divide_trees```).  The output will list a peak, its prominence
parent, and its line parent on each line.  Landmass high points (where the prominence is equal to the elevation) are not included.  
Their key saddles are the ocean, and there isn't a well-defined way to connect such peaks to other land masses through the divide tree.

## Anti-prominence

The "anti-prominence" of low points can be computed by the same algorithm, simply by changing
the sign of the elevation values.  This can be done by giving the -a option to the 
```prominence``` command.  Then, at the final stage of merging (with the -f flag), add the -a option 
again to flip the elevation values back to positive.

## More information

Explanations of what these calculations are about are at
http://andrewkirmse.com/isolation and
http://andrewkirmse.com/prominence, including nice visualizations.

This work was later published in the October, 2017 issue of the
journal "Progress of Physical Geography"
[here](https://doi.org/10.1177/0309133317738163).  The article can be
read [here](http://www.andrewkirmse.com/prominence-article).
