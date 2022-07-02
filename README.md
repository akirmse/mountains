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

NED (National Elevation Dataset) data is delivered as zip files with names like:

```
n59e006.zip
```

The prominence code will extract the FLT file from the zip (using 7z on Windows or unzip on Mac/Linux).

#### GLO30

The data is delivered as TIFF files.  Convert them to FLT using ```gdal_translate```, for example:

```
gdal_translate -of EHdr Copernicus_DSM_COG_10_N59_00_E006_00_DEM.tif Copernicus_DSM_COG_10_N59_00_E006_00_DEM.flt 
```

There is a script in the ```scripts``` subdirectory that can automate the downloading and conversion of tiles,
followed by running the prominence program on them.

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

### Prominence

First, generate divide trees for tiles of interest:

```
prominence -- <min latitude> <max latitude> <min longitude> <max longitude>

  Options:
  -i directory      Directory with terrain data
  -o directory      Directory for output data
  -f format         "SRTM", "NED13-ZIP", "NED1-ZIP", "GLO30" input files
  -k filename       File with KML polygon to filter input tiles
  -m min_prominence Minimum prominence threshold for output, default = 300ft
  -t num_threads    Number of threads, default = 1
```

This will produce divide trees with the .dvt extension, and KML files
that can be viewed in Google Earth.  The unpruned trees are generally
too large to be merged or to load into Earth.  Use the pruned versions
(identified by "pruned" in their filenames).

Next, merge the resultant divide trees into a single, larger divide
tree.  If there are thousands of input files, it will be much faster
to do this in multiple stages.

```
merge_divide_trees output_file_prefix input_file [...]
  Input file should have .dvt extension
  Output file prefix should have no extension

  Options:
  -f                Finalize output tree: delete all runoffs and then prune
  -m min_prominence Minimum prominence threshold for output, default = 300ft
```

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


## Results

The isolation file has one peak per line, in this format:

latitude,longitude,elevation in feet,ILP latitude,ILP longitude,isolation in km

where ILP means isolation limit point.

A zip file with our isolation results for the world is [here](https://drive.google.com/file/d/0B3icWNhBosDXRm1pak56blp1RGc/view?usp=sharing).

The prominence file has one peak per line, in this format:

latitude,longitude,elevation in feet,key saddle latitude,key saddle longitude,prominence in feet

A zip file with our prominence results for the world is [here](https://drive.google.com/file/d/0B3icWNhBosDXZmlEWldSLWVGOE0/view?usp=sharing).

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
  -m min_prominence Minimum prominence threshold for output, default = 300ft
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
