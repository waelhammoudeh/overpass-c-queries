/*
 * help.c
 *
 *  Created on: Jan 18, 2022
 *      Author: wael
 */
#include <stdio.h>
#include <stdlib.h>
#include "ztError.h"

extern char *prog_name;

void printHelp(FILE *toStream){

	char *helpTxt = "\n"
	"%s: Program queries Overpass server to fetch and display GPS coordinates for\n"
	"given cross road street names and bounding box.\n"
	"Program needs a connection to an Overpass server, the server is defined here\n"
	"to be on the local machine; see source file \"xrds2gps.h\" for definition.\n"
	"Program reads input from a text file - see format below, and displays output\n"
	"to stdout (screen) in table form with street names and GPS coordinates.\n\n"

	"Usage: xrds2gps [options] inputfile [files ...]\n\n"

	"Where \"inputfile\" is a required argument specifying the input file name to\n"
	"process or a list of files, space separated.\n"
	"And \"options\" are as follows:\n\n"

	"  -h   --help              Displays full program description.\n"
	"  -o   --output filename   Writes output to specified \"filename\"\n"
	"  -f   --force             Use with output option to force overwriting existing \"filename\"\n"
	"  -W   --WKT filename      Writes Well Known Text to \"filename\"\n"
	"  -r   --raw-data filename Writes received (downloaded) data from server to \"filename\"\n\n"

	"  Output directory: On invocation program creates a directory entry with program\n"
	"name \"xrds2gps\" under the effective user home directory, this directory is\n"
	"used for all program output files specified as single file name with any output\n"
	"option, if \"filename\" includes file system path then that is used instead.\n"
	"Currently this behaviour can not be changed.\n\n"

	"  Program options: Options can be specified with short or long form, \"filename\"\n"
	"after the option indicates a file name argument is required for that option.\n\n"

	" --help : displays this help information.\n\n"

	" --output filename : By default, program writes its output to stdout, using this\n"
	"                     option instructs program to write its output to \"filename\".\n"
	"                     Note: program output is the table with street names and the\n"
	"                     GPS coordinates.\n\n"

	" --force : By default, program will not overwrite existing file, by using this\n"
	"           option user can force program to change this default.\n\n"

	" --WKT filename : Tells the program to produce \"Well Known Text\" formated file\n"
	"                  in \"filename\".\n"
	"                  Note that using \".csv\" file extension makes it easier for QGIS\n"
	"                  to find your file.\n\n"
	" --raw-data filename : Program uses curl library memory download, query results\n"
	"                       are not saved to disk. Using this option user can save\n"
	"                       query results to \"filename\".\n\n"

	"  Input file list: In one invocation or session, program can process multiple\n"
	"files with space separated list. Program process each input file and the output\n"
	"is combined for all input files. If you have a large area, this a way to use\n"
	"smaller bounding box and speed up queries!\n"
	"Please do not list the same input file twice; there is no checking for duplicates!\n\n"

	"  Input file format : \n\n"
	"Input file is a text file with bounding box in first line, followed by a list\n"
	"of street names pairs, one pair per line comma separated.\n"
	"The bounding box is defined with two points in decimals; south-west point and\n"
	"north-east point listed with latitude then longitude and comma separated.\n"
	"Bounding box format: [ swLatitude, swLongitude, neLatitude, neLongitude ]\n\n"
	"Any line starting with hash-mark '#' or semi-colon ';' is considered a comment\n"
	"line and ignored.\n\n"
	"Below is an example of input file:\n\n"
	" # this is a comment line, will be ignored\n"
	" ; Bounding box {BBOX} below\n"
	" 33.53100, -112.07400, 33.56050, -112.0567345\n"
	"\n"
	" # Blank lines are allowed; like above.\n"
	" # start cross roads list\n"
	" North Central Avenue, East Butler Drive\n"
	" North Central Avenue, East Northern Avenue\n"
	" North Central Avenue, East Orangewood Avenue\n"
	" North Central Avenue, East Glendale Avenue\n"
	" North Central Avenue, East Maryland Avenue\n\n";

	fprintf (toStream, helpTxt, prog_name);
	return;

}

void shortUsage (FILE *toFP, ztExitCodeType exitCode){

	fprintf (stderr, "\n%s: Cross road to GPS; program fetches and displays GPS coordinates for cross roads.\n",
			      prog_name);
	fprintf (toFP, "Usage: %s [options] inputfile [files ...]\n\n", prog_name);
	fprintf (toFP, "Where \"inputfile\" is a required argument specifying the input file name to read (process)\n"
			"or a list of files, space separated for the program.\n"
			"And \"options\" are as follows:\n\n"
			"  -h   --help              Displays full program description.\n"
            "  -o   --output filename   Writes output to specified \"filename\".\n"
			"  -f   --force             Use with output option to force overwriting existing \"filename\".\n"
			"  -W   --WKT filename      Writes Well Known Text to \"filename\"\n"
			"  -r   --raw-data filename Writes received (downloaded) data from server to \"filename\".\n"
			"\n"
			"Try: '%s --help'\n\n", prog_name);


	exit (exitCode);

}

