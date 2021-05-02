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
#include <errno.h> // error number used with fopen() call

#include <curl/curl.h>
#include "xrds2gps.h"

#include "overpass-c.h"
#include "util.h"
#include "ztError.h"
#include "fileio.h" // this includes dList.h on top
#include "network.h"
#include "url_parser.h"
#include "curl_func.h"
#include "op_string.h"


const char *prog_name;  // prog_name is global
FILE	*rawDataFP = NULL; // wrong place for this FIXME

int main(int argc, char* const argv[]) {

	char *infile_desc = "Usage: %s inputfile\n"
			" where inputfile is a text file with BBOX in first line, followed by a list of\n"
			" street names pair, one pair (two names) per line comma separated.\n"
			" Below is an example of input file:\n\n"
			" # this is a comment line, will be ignored\n"
			" # when URL encoded the point order is: (longitude, latitude)\n"
			" 33.53100, -112.07400, 33.56050, -112.0567345\n"
			"\n"
			" # cross roads\n"
			" North Central Avenue, East Butler Drive\n"
			" North Central Avenue, East Northern Avenue\n"
			" North Central Avenue, East Orangewood Avenue\n"
			" North Central Avenue, East Glendale Avenue\n"
			" North Central Avenue, East Maryland Avenue\n\n";

	int				result;
	DL_LIST		*infileList;
	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	BBOX			bbox;
	XROADS		*xrds;
	DL_LIST		*xrdsList;
	char				*infile,
						*myString,
						*tempOut,
						*home,
						prog_dir[PATH_MAX] = {0}, // program output directory
						outdir[PATH_MAX] = {0};

	int		reachable;
	char		*ipBuf;
	char		*service_url,
				*serverOnly,
				*proto; // protocol

	struct parsed_url 	*urlParsed = NULL;

	/* set program name, used for feedback ...
	lastOfPath(), we might get called with a path */

	prog_name = lastOfPath (argv[0]);

	// we expect input file name after program name; is it missing?
	if (argc < 2){

		printf ("\n%s ERROR: missing input file name argument.\n\n", prog_name);
		printf (infile_desc, prog_name);
		return EXIT_FAILURE;
	}

	// is input file usable?
	infile = argv[1];
	if (IsArgUsableFile(infile) != ztSuccess){
		printf("%s error: input file <%s> error.\n", prog_name, infile);
		printf( " The error was: %s\n",
				    code2Msg(IsArgUsableFile(infile)));

		return EXIT_FAILURE;
	}

	/* parse the SERVICE_URL -- using parse_url() function here.
	 * Note that there is a libcurl function doing the same thing!
	 * curl_url_get() was added in version 7.62.0
	 */
	service_url = strdup(SERVICE_URL);
	if(IsSlashEnding(service_url)){            // I have made this mistake more than once!
		service_url[strlen(service_url) - 1] = '\0';
		printf("%s Warning Removed SLASH ending service!\n "
				"\t SERVICE_URL: %s\n \t service_url: %s\n\n",
				   prog_name, SERVICE_URL, service_url);
	}
	urlParsed = parse_url(service_url);
	if (NULL == urlParsed){

		printf("%s error: FROM parse_url() function.\n", prog_name);
		return EXIT_FAILURE;
	}
	else {

		printf("Parsed URL with parse_url(): \n");
		printf("  scheme is: %s\n", urlParsed->scheme);
		printf("  host is: %s\n", urlParsed->host);
		printf (" path is: %s\n", urlParsed->path);

	}

	/* can we reach the server?
	 * first parameter to checkURL() is the server name only,
	 * without the protocol .... */
	serverOnly = urlParsed->host;
	proto = urlParsed->scheme;
	reachable = checkURL (serverOnly , proto, &ipBuf);
	if (reachable != ztSuccess){

		printf("%s: Error SERVER: (%s) is NOT reachable.\n", prog_name, serverOnly);
		printf( " The error was: %s\n", code2Msg(reachable));

		return EXIT_FAILURE;
	}
	else
		printf("%s: REACHED SERVER: (%s).\n", prog_name, serverOnly);

	if (ipBuf)
		free(ipBuf); // it was needed just for function call.


	// prepare list to read file into list -- so we can parse the input file
	infileList = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (infileList == NULL){
		printf("%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}

	initialDL (infileList, zapLineInfo, NULL); // no return value to check ...

	/* file2List() fills the list with lines from text file,
	   ignoring lines starting with # or ; */
	result = file2List(infileList, infile);
	if (result != ztSuccess){
		printf("%s: Error returned by file2List()!\n", prog_name);
		printf (" The error from file2List() was: %s ... Exiting.\n",
				    code2Msg(result));
		return result;
	}

	// input file should have at least 2 lines: bbox + one crossing pair
	if (DL_SIZE(infileList) < 2){
		printf("%s: Error empty or incomplete input file.\n", prog_name);
		printf (infile_desc, prog_name);
		return EXIT_FAILURE;
	}

	/* lineInfo is a pointer to a structure with members:
			orginalNum: original line number and
	 	 	string: line contents ****/

	elem = DL_HEAD(infileList);
	lineInfo = (LINE_INFO *) elem->data;

	// get our own copy, since it gets mangled by strtok()
	myString = strdup (lineInfo->string);

	result = parseBbox (&bbox, myString);
	if (result != ztSuccess){
		printf("%s: Error parsing BBOX!\n\n", prog_name);
		printf("Expected bounding box format:\n"
				   "	swLatitude, swLongitude, neLatitude, neLongitude\n\n");
		return EXIT_FAILURE;
	}

	/* stick XROADS structures in a list */
	xrdsList = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (xrdsList == NULL){
		printf("%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}

	initialDL (xrdsList, NULL, NULL);

	// point at second line - this is the INPUT FILE list
	elem = DL_NEXT(elem);
	while (elem) {

		lineInfo = (LINE_INFO*) elem->data;

		if ((xrds = (XROADS*) malloc(sizeof(XROADS))) == NULL) {
			printf("%s: Fatal memory allocation!\n", prog_name);
			return ztMemoryAllocate;
		}
		memset(xrds, 0, sizeof(XROADS));

		myString = strdup(lineInfo->string);
		result = xrdsParseNames(xrds, myString);
		if (result != ztSuccess) {
			printf("%s: Error parsing cross roads line # %d "
					"from function xrdsParseNames().\n", prog_name,
					lineInfo->originalNum);
			return result;
		}

		// insert next to the end of the list
		insertNextDL (xrdsList, DL_TAIL(xrdsList), xrds);

		elem = DL_NEXT(elem);
	}// End while(elem)

	/* we have a DL_LIST (double linked list) filled with cross roads names AND
	   bounding box (BBOX), now we can send a query to our SERVER ....
	   Wait! we also need to save response to a FILE.

	  wget output is saved under a directory with "prog_name", for each input
	  file a directory is created under "prog_name" with input file name only -
	  we drop the extension. The output file name is made from input file name
	  only with element number appended to it.
	 **************************************************************************/

	home = getHome();
	ASSERTARGS(home);

	if(IsSlashEnding(home))
		sprintf (prog_dir, "%s%s", home, prog_name);
	else
		sprintf (prog_dir, "%s/%s", home, prog_name);

	result = myMkDir(prog_dir); // creates directory if it does not exist.
	if (result != ztSuccess){

		printf("%s: Error. Failed to create program output directory.\n", prog_name);
		printf( " The error was: %s\n", code2Msg(reachable));
		return result;
	}

	// create a directory with input file name only
	tempOut =  lastOfPath(infile);
	tempOut = DropExtension(tempOut);
	ASSERTARGS(tempOut);

	// not the best use for ASSERTARGS()!!!
	ASSERTARGS( snprintf(outdir, PATH_MAX,
			              "%s/%s", prog_dir, tempOut) < PATH_MAX);

	result = myMkDir(outdir);
	if (result != ztSuccess){

		printf("%s: Error. Failed to create output directory at < %s >.\n",
				prog_name, outdir);
		printf( " The error was: %s\n", code2Msg(reachable));
		return result;
	}

	//result = getXrdsDL (xrdsList, &bbox, service_url, outdir);
	result = curlGetXrdsDL (xrdsList, &bbox, service_url, outdir);
	if (result != ztSuccess){
		printf("%s: Error returned from getXrdsDL() !!!\n", prog_name);
		printf("FROM main() See FIRST error ^^^^  exiting\n\n");
		return EXIT_FAILURE;
	}

	printf("%s is DONE successfully. Find output files in directory:\n"
			  "\t %s\n\n", prog_name, outdir);

	// clean up
	destroyDL(infileList);
	free(infileList);

	destroyDL(xrdsList);
	free(xrdsList);
/*
	if (myString)
		free(myString);
*/
	if (urlParsed)
		parsed_url_free(&urlParsed);

//int getStreetNames (BBOX *bbox, char *server, char *dstFile)

//	getStreetNames (&bbox, service_url, "somefilename");

	return EXIT_SUCCESS;

} // End main()

int getXrdsDL(DL_LIST *xrdsDL, BBOX *bbox, char *server, char *outDir){

	DL_ELEM		*elem;
	XROADS		*xrds;
	char				*outPrefix;
	char				outFile[PATH_MAX] = {0}; // wget output file name
	char				outFileALL[PATH_MAX] = {0}; // result for all in input file
	int				elemNum = 1;
	FILE				*outALL;
	int				result;

	//do not allow nulls
	ASSERTARGS(xrdsDL && bbox && server && outDir);

	if(DL_SIZE(xrdsDL) == 0) // not even a warning

		return ztSuccess;

	// stitch (ALL) output file name; {INPUTFILE}-WGET_ALL
	outPrefix = lastOfPath(outDir); //this is input file name only - should be
	if (IsSlashEnding(outDir))
		sprintf(outFileALL, "%s%s-WGET_ALL", outDir, outPrefix);
	else
		sprintf(outFileALL, "%s/%s-WGET_ALL", outDir, outPrefix);

	errno = 0;
	outALL = fopen(outFileALL, "w"); // open file for writing

	if ( outALL == NULL){
		printf ("getXrdsDL(): Error could not create destination file! <%s>\n", &(outFileALL[0]));
		printf("  ^^ System error message: %s\n\n", strerror(errno));
		return ztCreateFileErr;
	}

	fprintf (outALL, " Elem #                              Cross Roads                              GPS (Longitude, Latitude)  Nodes Found\n");
	fprintf (outALL, "====================================================================================================================\n\n");

	elem = DL_HEAD(xrdsDL);
	while(elem){

		xrds = (XROADS *) elem->data;

		// output file for 2 streets is: {INPUTFILE}_elemNum
		if (IsSlashEnding(outDir))
			sprintf(outFile, "%s%s_%d", outDir, outPrefix, elemNum);
		else
			sprintf(outFile, "%s/%s_%d", outDir, outPrefix, elemNum);

		result = getXrdsGPS (xrds, bbox, server, outFile, parseWgetXrdsFile);
		if (result != ztSuccess){
			printf("getXrdsDL(): Error returned from getXrdsGPS() function\n\n");
			return result;
		}

		//write formated
		char name[71] = {0};
		char gps[36] = {0};
		sprintf(name, "[ %s & %s ]", xrds->firstRD, xrds->secondRD);
		sprintf(gps, "(%10.7f, %10.7f)", xrds->point.longitude, xrds->point.latitude);
		if (xrds->nodesFound == 0)
			sprintf(gps, " ----    Not Found   ---- ");

		fprintf (outALL, "%6d  %-68s %26s %6d\n",
				      elemNum, name, gps, xrds->nodesFound);

		elemNum++;
		elem = DL_NEXT(elem);

	} //end while(elem)

	fclose(outALL);

	return ztSuccess;
}

int curlGetXrdsDL(DL_LIST *xrdsDL, BBOX *bbox, char *server, char *outDir){

	DL_ELEM		*elem;
	XROADS		*xrds;
	char				*outPrefix;
	char				outFileALL[PATH_MAX] = {0};
	int				elemNum = 1;
	FILE				*outFP;
	int				result;

#ifdef DEBUG_QUERY
	char				rawDataFileName[PATH_MAX] = {0};
#endif

	//do not allow nulls
	ASSERTARGS(xrdsDL && bbox && server && outDir);

	if(DL_SIZE(xrdsDL) == 0) // not even a warning

		return ztSuccess;

	result = curlInitialSession();
	if (result != 0){

		printf("curlGetXrdsDL(): Error returned from curlInitialSession().\n");
		return ztUnknownError;
	}

	// stitch output file name; {INPUTFILE}-CURL_ALL
	outPrefix = lastOfPath(outDir); //this is input file name only - should be
	if (IsSlashEnding(outDir))
		sprintf(outFileALL, "%s%s-CURL_ALL", outDir, outPrefix);
	else
		sprintf(outFileALL, "%s/%s-CURL_ALL", outDir, outPrefix);

	errno = 0;
	outFP = fopen(outFileALL, "w");

	if ( outFP == NULL){
		printf ("curlGetXrdsDL(): Error could not create destination file! <%s>\n", &(outFileALL[0]));
		printf("  ^^ System error message: %s\n\n", strerror(errno));
		return ztCreateFileErr;
	}

	/* make raw-data file name - raw-data file is created only if we want to
		debug query output, raw data will be written when query is done by
		curlGetXrdsGPS() function. We use one file for the session.
		The file name will be {INPUTFILE}-RAW_DATA (input file name only with
		"RAW_DATA" appended to it, and created in the session output directory
		The rawDataFP - raw data file pointer is global!
		Created "xrds2gps.h" to make globals available for files that include it,
		The idea is right ... but if we were to make a library for curl functions
		this is NOT the right place for our defines and globals! FIXME
	*/
#ifdef DEBUG_QUERY

	if (IsSlashEnding(outDir))
		sprintf(rawDataFileName, "%s%s-RAW_DATA", outDir, outPrefix);
	else
		sprintf(rawDataFileName, "%s/%s-RAW_DATA", outDir, outPrefix);

	errno = 0;
	rawDataFP = fopen(rawDataFileName, "w");

	if ( rawDataFP == NULL){
		printf ("curlGetXrdsDL(): Error could not create destination file! <%s>\n", &(rawDataFileName[0]));
		printf("  ^^ System error message: %s\n\n", strerror(errno));
		return ztCreateFileErr;
	}

	fprintf (rawDataFP, "This is the raw data received from curl query.\n");

#endif

	// write heading in output file
	fprintf (outFP, " Elem #                              Cross Roads                              GPS (Longitude, Latitude)  Nodes Found\n");
	fprintf (outFP, "====================================================================================================================\n\n");

	elem = DL_HEAD(xrdsDL);
	while(elem){

		xrds = (XROADS *) elem->data;

		// try me with both methods; Post & Get. One at a time!
		//result = curlGetXrdsGPS (xrds, bbox, server, Post, parseCurlXrdsData);
		result = curlGetXrdsGPS (xrds, bbox, server, Get, parseCurlXrdsData);
		if (result != ztSuccess){
			printf("curlGetXrdsDL(): Error returned from curlGetXrdsGPS() function\n\n");
			return result;
		}

		//write result formated
		char name[71] = {0};
		char gps[36] = {0};
		sprintf(name, "[ %s & %s ]", xrds->firstRD, xrds->secondRD);
		sprintf(gps, "(%10.7f, %10.7f)", xrds->point.longitude, xrds->point.latitude);
		if (xrds->nodesFound == 0)
			sprintf(gps, " ----    Not Found   ---- ");

		fprintf (outFP, "%6d  %-50s %26s %6d\n",				// name field 50 from 68
				      elemNum, name, gps, xrds->nodesFound);

		elemNum++;
		elem = DL_NEXT(elem);

	} //end while(elem)

	fclose(outFP);

#ifdef DEBUG_QUERY

	fflush(rawDataFP);
	fclose(rawDataFP);

#endif

	/* start of get street name here. maybe not the best place for it, but at
	 * this point we have all we need to show an example usage: we have
	 * the bounding box "bbox" the URL to server "server" and we have not
	 * closed curl session yet. we need an output file name!
	 */


// getStreetNames (bbox, server, "somefilename"); needs own program!

	curlCloseSession();

	return ztSuccess;
}
