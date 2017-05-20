# Prominence and isolation

C++ programs for calculating the isolation and prominence from digital
elevation data.

## Building the code

### OSX and gcc

Debug version:
make  

Release version:
RELEASE=1 make



### Windows

nmake -f makefile.win

## Running the code

## Results

In the results subdirectory are the output of running this code over
the entire world. The isolation file has one peak per line, in this
format:

latitude,longitude,elevation in feet,ILP latitude,ILP longitude,isolation in km

where ILP means isolation limit point.

The prominence file has one peak per line, in this format:

latitude,longitude,elevation in feet,key saddle latitude,key saddle longitude,prominence in feet


## More information

Explanations of what these calculations are about are at
http://andrewkirmse.com/isolation and
http://andrewkirmse.com/prominence, including nice visualizations.
