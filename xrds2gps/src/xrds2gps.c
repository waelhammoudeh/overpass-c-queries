/*
 ============================================================================
 Name        : xrds2gps.c
 Author      : Wael Hammoudeh
 Version     :
 Copyright   : Your copyright notice
 Description : Cross Roads To GPS: Program reads a text file with BBOX and cross
                      roads list (two street names), returns GPS if found, else not found.
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h> // GNU getopt_long() use
#include <unistd.h> // access() call
#include <errno.h> // error number used with fopen()

#include "xrds2gps.h"
#include "ztError.h"
#include "network.h" // has checkURL()
#include "overpass-c.h"
#include "util.h"
#include "ztError.h"
#include "fileio.h" // this includes dList.h on top
#include "url_parser.h"
#include "curl_func.h"
#include "op_string.h"

// prog_name is global
const char *prog_name;
FILE *rawDataFP = NULL;

// function prototype
static int appendToDL (DL_LIST *dest, DL_LIST *src);

int main(int argc, char* const argv[]) {

	char		*home,
				progDir[PATH_MAX] = {0}; // program output directory

	// getopt_long() variables
	const 	char*	const	shortOptions = "ho:r:W:f";
	const	struct	option	longOptions[] = {
			{"help", 		0, NULL, 'h'},
			{"output", 	1, NULL, 'o'},
			{"raw-data", 1, NULL, 'r'},
			{"WKT", 1, NULL, 'W'},
			{"force", 0, NULL, 'f'},
			{NULL, 0, NULL, 0}

	};

	int			result;
	char			**argvPtr;
	int			nextOption;
	int			overWrite = 0;	  // do not over write existing file

	char			*outputFileName = NULL;
	char			*rawDataFileName = NULL;
	char			*wktFileName = NULL;

	struct 		parsed_url 	*urlParsed = NULL;
	char			*service_url,
					*serverOnly,
					*proto,
					*ipBuf;
	int			reachable;

	char				*infile;
	DL_LIST		*infileList; // data pointer in element is to LINE_INFO
	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	char				*myString;
	BBOX			bbox;
	XROADS		*xrds;
	DL_LIST		*xrdsList; // data pointer in element is to XROADS
	DL_LIST		*xrdsSessionDL; // session list of XROADS
	DL_LIST		*wktDL;

	FILE		*outputFilePtr = NULL;
	FILE		*wktFilePtr = NULL;

	char		wktFileNameCSV[PATH_MAX] = {0};

	/* set prog_name .. lastOfPath() might get called with a path */
	prog_name = lastOfPath (argv[0]);

	/* missing required argument - show usage, exit with ztMissingArgError */
	if (argc < 2)

		shortUsage(stderr, ztMissingArgError);

	// set program output directory
	home = getHome();
	ASSERTARGS(home);

	if(IsSlashEnding(home))
		// size = (PATH_MAX - 100) allow for file name
		snprintf (progDir, (PATH_MAX - 100), "%s%s", home, prog_name);
	else
		snprintf (progDir, (PATH_MAX - 100), "%s/%s", home, prog_name);

	result = myMkDir(progDir); // creates directory if it does not exist.
	if (result != ztSuccess){

		fprintf(stderr, "%s: Error. Failed to create program output directory.\n", prog_name);
		fprintf(stderr, " The error was: %s\n", code2Msg(result));
		return result;
	}

	/* parse command line with getopt_long() */
	do {

		nextOption = getopt_long(argc, argv, shortOptions, longOptions, NULL);

		switch (nextOption){

		case 'h': // show help and exit.

			printHelp(stdout);
			return ztSuccess;
			break;

/*		case 'v':
			//verbose = 1;
			break;
*/
		case 'f':

			overWrite = 1;
			break;

		case 'o':

			/* optarg points at output file name; note that more testing
			 * on output file is done after getopt_long() is done! *****/

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for output: <%s>\n",
						    prog_name, optarg);
				return ztBadFileName;
			}

			// make output file name -- NEEDS checking!!
			mkOutputFile (&outputFileName, optarg, progDir);

			break;

		case 'r':

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for raw-data: <%s>\n",
						    prog_name, optarg);
				return ztBadFileName;
			}

			// make raw data file name
			mkOutputFile (&rawDataFileName, optarg, progDir);

			break;

		case 'W':

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for WKT: <%s>\n",
						    prog_name, optarg);
				return ztBadFileName;
			}

			// make WKT file name.
			/* Note we add '.csv' extension to file name before we open it below */
			mkOutputFile (&wktFileName, optarg, progDir);

			break;

		case '?':

			fprintf (stderr, "%s: Error unknown option specified.\n", prog_name);
			shortUsage (stderr, ztUnknownOption); // show usage and exit.

			break;

		case -1: // done with options

			break;

		default : // something unusual? who knows?

			abort();

		} // end switch (nextOption)

	} while ( nextOption != -1 );  // end do ... while() getopt_long() returns -1 when done

	// getopt_long() is done,  optind is current argv[] index --

	if (optind == argc){ // nothing is left for required input filename

		fprintf (stderr, "%s: Error missing required argument for input file name.\n",
				    prog_name);
		shortUsage(stderr, ztMissingArgError);
	}

	/* No server -> no service! I do this after getopt_long() to enable the help
	 * option when we do not have a connection to server!
	 * Can we connect to Overpass server?
	 * parse the SERVICE_URL
	 * Note that there is a libcurl function doing the same thing!
	 * curl_url_get() was added in version 7.62.0
	 */
	service_url = strdup (SERVICE_URL);

	urlParsed = parse_url(service_url);
	if (NULL == urlParsed){

		fprintf(stderr, "%s error: FROM parse_url() function.\n", prog_name);
		return ztParseError;
	}

	serverOnly = urlParsed->host;
	proto = urlParsed->scheme;
	result = checkURL (serverOnly , proto, &ipBuf); /* network.c */
	reachable = (result == ztSuccess);
	if ( ! reachable ){

		fprintf(stderr, "%s: Error SERVER: (%s) is NOT reachable.\n",
				     prog_name, serverOnly);
		fprintf(stderr, " The error was: %s\n", code2Msg(result));

		return result;
	}

	if (ipBuf)
		free(ipBuf); // it was needed just for function call.

	// do not over write existing output file, unless force was used
	if ( outputFileName &&
		 (access(outputFileName, F_OK) == 0) && ! overWrite ){

		fprintf (stderr, "%s: Error specified output file <%s> already exists. "
				"Use \"force\" option to over write it.\n", prog_name, outputFileName);
		return ztInvalidArg;
	}

	// check files
	argvPtr= (char **) (argv + optind);
	while (*argvPtr){

		result = IsArgUsableFile(*argvPtr); // check input file
		if (result != ztSuccess){

			fprintf(stderr, "%s error: input file <%s> is Not usable file!\n",
					    prog_name, *argvPtr);
			fprintf(stderr, " The error was: %s\n", code2Msg(result));
			return result;
		}

		/* do not overwrite an INPUT file ANYTIME */
		if (outputFileName && (strcmp(*argvPtr, outputFileName) == 0)){
			fprintf(stderr, "%s Error: Can not write output to an input file: <%s>\n\n",
					prog_name, *argvPtr);
			return ztInvalidArg;
		}
		if (rawDataFileName && (strcmp(*argvPtr, rawDataFileName) == 0)){
			fprintf(stderr, "%s Error: Can not write raw data to an input file: <%s>\n\n",
					prog_name, *argvPtr);
			return ztInvalidArg;
		}
		if (wktFileName && (strcmp(*argvPtr, wktFileName) == 0)){
			fprintf(stderr, "%s Error: Can not write WKT to an input file: <%s>\n\n",
					prog_name, *argvPtr);
			return ztInvalidArg;
		}

		argvPtr++; // move to next argv

	} // end  while () check all files

	// open output file(s) for writing when name is set

	if (outputFileName) {

		outputFilePtr = openOutputFile (outputFileName);
		if ( ! outputFilePtr) {
			fprintf (stderr, "%s: Error opening output file: <%s>\n",
					      prog_name, outputFileName);
			return ztOpenFileError;
		}
	}

	if (wktFileName){

		/* csv (comma separated variables) extension to output file name ONLY
		 * when file name has no extension  */
		//char		wktFileNameCSV[PATH_MAX] = {0};

		if (strrchr(wktFileName, '.'))
			strcpy (wktFileNameCSV, wktFileName);
		else {
			sprintf (wktFileNameCSV, "%s.csv", wktFileName);
			fprintf (stdout, "%s: saving WKT output to file: %s\n", prog_name, wktFileNameCSV);
		}

		wktFilePtr = openOutputFile (wktFileNameCSV);
		if ( ! wktFilePtr) {
			fprintf (stderr, "%s: Error opening WKT output file: <%s>\n",
			             prog_name, wktFileName);
			return ztOpenFileError;
		}
	}

	if (rawDataFileName){ // writing is done by curlGetXrdsGPS() in overpass.c

		rawDataFP = openOutputFile (rawDataFileName);
		if ( ! rawDataFP ) {
			fprintf (stderr, "%s: Error opening raw data output file: <%s>\n",
					     prog_name, rawDataFileName);
			return ztOpenFileError;
		}
	}

	// initial a list for the session
	xrdsSessionDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (xrdsSessionDL == NULL){
		fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}
	initialDL (xrdsSessionDL, NULL, NULL);

	/* done setting & parsing, now process each input file */
	argvPtr = (char **) (argv + optind);

	while (*argvPtr){

		infile = *argvPtr;
		// prepare list to read file into list -- so we can parse the input file
		infileList = (DL_LIST *) malloc(sizeof(DL_LIST));
		if (infileList == NULL){
			fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
			return ztMemoryAllocate;
		}

		initialDL (infileList, zapLineInfo, NULL); // no return value to check

		/* file2List() fills the list with lines from text file,
		   ignoring lines starting with # or ; */
		result = file2List(infileList, infile);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error reading input file: %s \n",
					prog_name, infile);
			fprintf (stderr, " The error from file2List() was: %s ... Exiting.\n",
					    code2Msg(result));
			return result;
		}

		// input file should have at least 2 lines: bbox + one crossing pair
		if (DL_SIZE(infileList) < 2){
			fprintf(stderr, "%s: Error empty or incomplete input file.\n", prog_name);
			fprintf (stderr, "Please see input file format in help with: %s --help\n", prog_name);
			return EXIT_FAILURE;
		}

		/* get bounding box string and parse it */
		elem = DL_HEAD(infileList);
		lineInfo = (LINE_INFO *) elem->data;

		// get our own copy, since it gets mangled by strtok()
		myString = strdup (lineInfo->string);

		result = parseBbox (&bbox, myString);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error parsing BBOX!\n\n", prog_name);
			fprintf(stderr, "Expected bounding box format:\n"
					   "	swLatitude, swLongitude, neLatitude, neLongitude\n\n");
			return EXIT_FAILURE;
		}

		/* get cross road strings, parse them && stuff'em in a list */
		xrdsList = (DL_LIST *) malloc(sizeof(DL_LIST));
		if (xrdsList == NULL){
			fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
			return ztMemoryAllocate;
		}

		initialDL (xrdsList, NULL, NULL);

		// point at second line - this is the INPUT FILE list
		elem = DL_NEXT(elem);
		while (elem) {

			lineInfo = (LINE_INFO*) elem->data;

			if ((xrds = (XROADS*) malloc(sizeof(XROADS))) == NULL) {
				fprintf(stderr, "%s: Fatal memory allocation!\n", prog_name);
				return ztMemoryAllocate;
			}
			memset(xrds, 0, sizeof(XROADS));

			myString = strdup(lineInfo->string);
			result = xrdsParseNames(xrds, myString);
			if (result != ztSuccess) {
				fprintf(stderr, "%s: Error parsing cross roads line # %d "
						"from function xrdsParseNames().\n", prog_name,
						lineInfo->originalNum);
				return result;
			}

			// insert next to the end of the list
			insertNextDL (xrdsList, DL_TAIL(xrdsList), xrds);

			elem = DL_NEXT(elem);
		}// End while(elem)

		/* function getXrdsDL() uses wget binary to query server, this program
		 * shows how to use curl library functions ****/
		/* get GPS for cross roads list */
		result = curlGetXrdsDL (xrdsList, &bbox, service_url);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error returned from getXrdsDL() !!!\n", prog_name);
			fprintf(stderr, "See FIRST error above ^^^^  exiting\n\n");
			return EXIT_FAILURE;
		}

		// append this file list to session list
		appendToDL (xrdsSessionDL, xrdsList);

		destroyDL (xrdsList);
		free ((DL_LIST *) xrdsList);

		argvPtr++; // next input file?

	} // end  while (*argvPtr)

	writeDL (outputFilePtr, xrdsSessionDL, writeXrds);

	if (wktFileName){

		wktDL = (DL_LIST *) malloc (sizeof(DL_LIST));
		initialDL(wktDL, NULL, NULL);
		xrds2WKT_DL (wktDL, xrdsSessionDL);
		writeDL (wktFilePtr, wktDL, writeString2FP);

	}

	if (outputFilePtr) {

		fprintf (stdout, "Wrote output to file: %s\n", outputFileName);
		fclose (outputFilePtr);
	}

	if (wktFilePtr) {

		fprintf (stdout, "Wrote Well Known Text to file: %s\n", wktFileNameCSV);
		fclose (wktFilePtr);
	}

	if (rawDataFP) {

		fprintf (stdout, "Wrote raw data to file: %s\n", rawDataFileName);
		fclose (rawDataFP);
	}


	// all done
	return ztSuccess;

} // END main()

/* srcDL is a double linked list with XROADS* as data pointer in ELEM,
 * error to be empty!
 * dstDL : initialed by caller.
 */
int xrds2WKT_DL (DL_LIST *dstDL, DL_LIST *srcDL){

	DL_ELEM	*elem;
	XROADS	*xrds;
	char			**wktStrArray;
	char			**strMover;
	char			*wktMaker = "wkt;";

	ASSERTARGS (dstDL && srcDL);

	if (DL_SIZE(srcDL) == 0)

		return ztListEmpty;

	// start the list with wkt maker
	insertNextDL (dstDL, DL_TAIL(dstDL), wktMaker);

	elem = DL_HEAD(srcDL);
	while(elem){

		xrds = (XROADS *) elem->data;
		wktStrArray = (char **) malloc ((sizeof(char *)) * (xrds->nodesFound + 1));
		if ( ! wktStrArray){
			fprintf (stderr, "xrds2WKT_DL() Error: memory allocate!\n");
			return ztMemoryAllocate;
		}

		xrds2WKT (wktStrArray, xrds); //check result TODO

		strMover = wktStrArray;
		while (*strMover){

			insertNextDL (dstDL, DL_TAIL(dstDL), *strMover);
			strMover++;
		}

		elem = DL_NEXT(elem);

	} // end while(elem)

	return ztSuccess;

} // END xrds2WKT_DL()

void printHelp(FILE *toStream){

/**	char *helpTxt = "%s: Program queries Overpass server to fetch and display GPS coordinates\n"
			"for given cross road street names and bounding box.\n"
			"Program reads input from a text file - see format below, and displays output in column form\n"
			"with cross road street names and GPS coordinates.\n";
**/

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

/* appendDL () appends src list to dest list - assumes the data pointer in
 * element is for character string, no checking for that is done! */
static int appendToDL (DL_LIST *dest, DL_LIST *src){

	DL_ELEM	*srcElem;
	XROADS	*srcXrds;
	XROADS	*newXrds;
	int			result;

	ASSERTARGS (dest && src);

	if (DL_SIZE(src) == 0)

		return ztSuccess;

	srcElem = DL_HEAD(src);
	while (srcElem){

		srcXrds = (XROADS *) DL_DATA(srcElem);

		newXrds = srcXrds;

		result = insertNextDL (dest, DL_TAIL(dest), newXrds);
		if (result != ztSuccess)

			return ztMemoryAllocate;

		srcElem = DL_NEXT(srcElem);

	}

	return ztSuccess;
}

/* mkOutFile(): make output file name, sets dest to givenName if it has a slash,
 * else it appends givenName to rootDir and then sets dest to appended string
 */
int mkOutputFile (char **dest, char *givenName, char *rootDir){

	char		slash = '/';
	char		*hasSlash;
	char		tempBuf[PATH_MAX] = {0};

	ASSERTARGS (dest && givenName && rootDir);

	hasSlash = strchr (givenName, slash);

	if (hasSlash)

		*dest = (char *) strdup (givenName); // strdup() can fail .. check it FIXME

	else {

		if(IsSlashEnding(rootDir))

			snprintf (tempBuf, PATH_MAX -1 , "%s%s", rootDir, givenName);
		else
			snprintf (tempBuf, PATH_MAX - 1, "%s/%s", rootDir, givenName);

		*dest = (char *) strdup (&(tempBuf[0]));

	}

	/* mystrdup() ::: check return value of strdup() TODO */

	return ztSuccess;
}

int curlGetXrdsDL (DL_LIST *xrdsDL, BBOX *bbox, char *server){

	DL_ELEM		*elem;
	XROADS		*xrds;
	int				result;

	//do not allow nulls
	ASSERTARGS(xrdsDL && bbox && server);

	if(DL_SIZE(xrdsDL) == 0) // not even a warning

		return ztSuccess;

	result = curlInitialSession();
	if (result != 0){

		fprintf(stderr, "curlGetXrdsDL(): Error returned from curlInitialSession().\n");
		return ztUnknownError;
	}

	elem = DL_HEAD(xrdsDL);
	while(elem){

		xrds = (XROADS *) elem->data;

		// try me with both methods; Post & Get. One at a time!
		//result = curlGetXrdsGPS (xrds, bbox, server, Post, parseCurlXrdsData);
		result = curlGetXrdsGPS (xrds, bbox, server, Get, parseCurlXrdsData);
		if (result != ztSuccess){
			fprintf(stderr, "curlGetXrdsDL(): Error returned from curlGetXrdsGPS() function\n\n");
			return result;
		}

		elem = DL_NEXT(elem);

	} //end while(elem)

	curlCloseSession();

	return ztSuccess;

} // END curlGetXrdsDL()

/* openOutputFile(): opens filename for writing, filename includes path */

FILE* openOutputFile (char *filename){

	FILE		*fPtr = NULL;

	ASSERTARGS (filename);

	errno = 0; //set error number

	//try to open the file for writing
	fPtr = fopen(filename, "w");
	if (fPtr == NULL){

		fprintf (stderr, "openOutputFile(): Error opening file: <%s>\n", filename);
		fprintf(stderr, "System error message: %s\n\n", strerror(errno));
		//print reason with perror()
		perror("The call to fopen() failed!");
	}

	return fPtr;

} // END openOutputFile()

