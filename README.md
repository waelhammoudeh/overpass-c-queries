This repository is for OpenStreetMap (OSM) Overpass server usage; how to query
the server and how to use the OSM data.

An Overpass server can be installed on a local machine with limited area data,
which does not require a lot of hardware resources. Please see my guide for
installing on Linux Slackware here:
https://github.com/waelhammoudeh/overpass-4-slackware
So please do not over use the public overpass server(s). Please setup your own
server and use that with no limits, thank you in advance.

A query to Overpass server can be sent through script or program, I use c 
programming, hence, this repository also illustrates c programming.

Organization:

I try to write small functions and group related functions together in one
file. For example my linked list data structure and implementation are in the
"dList.c" file and its header file "dList.h", the file input / output handling
functions are in "fileio.c" and its header file "fileio.h". I try to use
descriptive names for files, functions and variables I use.

Some functions may seem to have similar names, but they are different; like:
getXrdsDL() vs. getXrds() for example.
Both functions are (in English) get cross roads. Note that the first ends
with "DL" suffix, it takes a Double Linked List as a parameter, it operates
on the list; where the second "getXrds()" takes one single variable of the
type, works on one.

Strange things (maybe!!):

I use Double Linked List for file I/O. A text file can be placed into a DL
with each line as a list element - in a structure like LINE_INFO sometimes or 
directly as a string (a pointer to character really). Take a look at the
implementation functions in file "fileio.c" for file2List(), liList2File() and
strList2File().

Overpass queries and related functions are in "overpass-c.c" file, this 
repository is about this file. Implemented queries are listed here in this
README.md file.

1) Cross Roads Query: 
   An overpass query to retrieve the GPS for specified roads. The query is
 implemented by the functions:
   getXrdsGPS() and curlGetXrdsGPS(). The first uses system "wget" command
   to query server and saves its response as a disk file. 
   The second uses "libcurl" for the query and processes response in memory.
   
2) Get Street Names Query:
   An overpass query to retrieve street / road names within a specified 
 bounding box. The query is implemented by getStreetNames() function.


Last; nothing is perfect and everything can be improved.

An Example:
 
 The "xrds2gps" directory contains an example program with a makefile to build.
 Note that the input file and data I use are for Phoenix, Arizona.
 I should mention the quick call to getStreetNames() in that example was just
 that quick lazy call. That query is simple for a reason, and it is worthy of
 its own example and documentation ... that is first in my TODO list.
 
 Modified xrds2gps program; getopt_long() is used to parse command line arguments.
 Edited help page.
  * New: Save raw data to disk option.
  * New: Write WKT (Well Known Text) formatted output option.

Well Known Text (WKT) formatted file can be used to visualize cross roads results
from Overpass query. It can be used with QGIS - maybe others - to draw points on
an Open Street Map window by adding a layer to the map using this WKT file.
QGIS is available for Linux, for Slackware a build script is available on:
www.slackbuilds.org.

With QGIS installed, from the top menu select:
  * Layer --> Add layer --> Add Delimited Text Layer ...
  * 
and then from the dialog box choose your file - it helps to name your file with
".csv" extension. In the file name field provide your file name or you may browse
to it. Once file is selected, the "Add" button on the lower right will be enabled
and you should be able to push to add the layer. There are a lot of options in 
that dialog, defaults should do just fine. Make your layer the top layer over open
street map.
Maybe somebody can add a howto for this!


Questions are welcome, send me am email or a tweet ...


Wael Hammoudeh
