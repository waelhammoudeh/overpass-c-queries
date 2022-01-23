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
#include "curl_func.h"
#include "op_string.h"
#include "help.h"

// prog_name is global
const char *prog_name;
FILE *rawDataFP = NULL;

// function prototype
static int appendToDL (DL_LIST *dest, DL_LIST *src);

int main(int argc, char* const argv[]) {

	int 		retCode = ztSuccess;
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

	char		wktFileNameExt[PATH_MAX] = {0}; // filename extension added

	char		*wktNameOnly;
	char 	*wktMidGpsName = NULL;
	char		*wktBboxName;
	char		tmpBuf[PATH_MAX];
	FILE		*wktMidGpsFilePtr = NULL;
	FILE		*wktBboxFilePtr = NULL;
	DL_LIST	*bboxWktDL;
	DL_LIST	*mgWktList;
//	DL_LIST	*mgWktSessionList;

	char		*bboxWktStr;

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
		retCode = result;
		goto cleanup;
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
				retCode = ztBadFileName;
				goto cleanup;
			}

			result = mkOutputFile (&outputFileName, optarg, progDir);
			if (result != ztSuccess){
				retCode = result;
				goto cleanup;
			}

			break;

		case 'r':

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for raw-data: <%s>\n",
						    prog_name, optarg);
				retCode = ztBadFileName;
				goto cleanup;
			}

			result = mkOutputFile (&rawDataFileName, optarg, progDir);
			if (result != ztSuccess){
				retCode = result;
				goto cleanup;
			}
			break;

		case 'W':

			if ( ! IsGoodFileName(optarg) ){
				fprintf (stderr, "%s: Error invalid file name specified for WKT: <%s>\n",
						    prog_name, optarg);
				retCode = ztBadFileName;
				goto cleanup;
			}

			/* Note we add '.wkt' extension to file name before we open it
			 * only when user did not give it one. */
			result = mkOutputFile (&wktFileName, optarg, progDir);
			if (result != ztSuccess){
				retCode = result;
				goto cleanup;
			}

			wktNameOnly = DropExtension(wktFileName);

			sprintf(tmpBuf, "%s_MidGps.wkt", wktNameOnly);
			mkOutputFile (&wktMidGpsName, tmpBuf, progDir);

			sprintf(tmpBuf, "%s_Bbox.wkt", wktNameOnly);
			mkOutputFile (&wktBboxName, tmpBuf, progDir);

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
	if ( ! service_url ){
		fprintf(stderr, "%s error: Got NULL from strdup() function.\n", prog_name);
		retCode = ztGotNull;
		goto cleanup;
	}

	/* use CURL parser. initial curl session checks version number */
	result = initialSession();
	if (result != ztSuccess){
		fprintf(stderr, "%s error: Could not initial curl session. Exiting!\n", prog_name);
		retCode = result;
		goto cleanup;
	}

	CURLU *url = initialURL (service_url);
	CURLUcode rc;

	if (  ! url ){
		fprintf(stderr, "%s error: Got NULL from initialURL() function.\n", prog_name);
		retCode = ztGotNull;
		goto cleanup;
	}

	/* get serverOnly and proto */
	rc = curl_url_get (url, CURLUPART_HOST, &serverOnly, 0);
	if (rc != CURLUE_OK){

		fprintf(stderr, "%s: Could get server name, curl_url_get() failed: %s\n",
				      prog_name, curl_url_strerror(rc));
		retCode = ztParseError;
		goto cleanup;
	}

	rc = curl_url_get (url, CURLUPART_SCHEME, &proto, 0);
	if (rc != CURLUE_OK){

		fprintf(stderr, "%s: Could get proto, curl_url_get() failed: %s\n",
				      prog_name, curl_url_strerror(rc));
		retCode = ztParseError;
		goto cleanup;
	}

	result = checkURL (serverOnly , proto, &ipBuf); /* network.c */
	reachable = (result == ztSuccess);
	if ( ! reachable ){

		fprintf(stderr, "%s: Error SERVER: (%s) is NOT reachable.\n",
				     prog_name, serverOnly);
		fprintf(stderr, " The error was: %s\n", code2Msg(result));

		retCode = result;
		goto cleanup;
	}

	if (ipBuf)
		free(ipBuf); // it was needed just for function call.

	/* no longer need both serverOnly and proto */
	curl_free(serverOnly);
	curl_free(proto);

	// do not over write existing output file, unless force was used
	if ( outputFileName &&
		 (access(outputFileName, F_OK) == 0) && ! overWrite ){

		fprintf (stderr, "%s: Error specified output file <%s> already exists. "
				"Use \"force\" option to over write it.\n", prog_name, outputFileName);
		retCode = ztInvalidArg;
		goto cleanup;
	}

	// check files
	/* TODO do not allow wkt or csv extension in output file name.
	 * do not use the same output file name twice.
	 */
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

		/* when file name has no extension, we add "wkt" as extension to file name */
		if (strrchr(wktFileName, '.'))
			strcpy (wktFileNameExt, wktFileName);
		else {
			sprintf (wktFileNameExt, "%s.wkt", wktFileName);
			fprintf (stdout, "%s: saving WKT output to file: %s\n", prog_name, wktFileNameExt);
		}

		if (outputFileName && (strcmp(outputFileName, wktFileNameExt) == 0)){

			fprintf(stderr, "%s Error: Same file name used for output option and wktFileName: <%s>\n\n",
					prog_name, wktFileNameExt);
			return ztInvalidArg;
		}

		wktFilePtr = openOutputFile (wktFileNameExt);
		if ( ! wktFilePtr) {
			fprintf (stderr, "%s: Error opening WKT output file: <%s>\n",
			             prog_name, wktFileName);
			return ztOpenFileError;
		}

		wktBboxFilePtr = openOutputFile (wktBboxName);
		if ( ! wktBboxFilePtr) {
			fprintf (stderr, "%s: Error opening WKT output file: <%s>\n",
			             prog_name, wktBboxName);
			return ztOpenFileError;
		}

		wktMidGpsFilePtr = openOutputFile (wktMidGpsName);
		if ( ! wktMidGpsFilePtr) {
			fprintf (stderr, "%s: Error opening WKT output file: <%s>\n",
			             prog_name, wktMidGpsName);
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

	/* initial a list for the session - this is a XROADS list,
	 * data in element is a pointer to XROADS */
	xrdsSessionDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (xrdsSessionDL == NULL){
		fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
		return ztMemoryAllocate;
	}
	initialDL (xrdsSessionDL, zapXrds, NULL);

	if (wktFilePtr){

		bboxWktDL = (DL_LIST *) malloc(sizeof(DL_LIST));
		if ( ! bboxWktDL ){
			fprintf(stderr, "%s: Error allocating memory for bboxWktDL.\n",
					prog_name);
			return ztMemoryAllocate;
		}
		initialDL (bboxWktDL, zapString, NULL);
		/* insert wkt; as first line */
		insertNextDL (bboxWktDL, DL_TAIL(bboxWktDL), "wkt;");

	}

	/* done setting & parsing, now process each input file */
	argvPtr = (char **) (argv + optind);

	while (*argvPtr){

		infile = *argvPtr;
		/* initial file list - this is a string list (LineInfo really),
		 * data pointer in element is a pointer to LineInfo structure */
		infileList = (DL_LIST *) malloc(sizeof(DL_LIST));
		if (infileList == NULL){
			fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
			return ztMemoryAllocate;
		}

		initialDL (infileList, zapLineInfo, NULL);

		/* file2List() function fills the list with lines from text file,
		   ignoring lines starting with # and ; */
		result = file2List(infileList, infile);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error failed file2List(): %s \n",
					prog_name, infile);
			fprintf (stderr, " The error from file2List() was: %s ... Exiting.\n",
					    code2Msg(result));
			return result;
		}

		// input file should have at least 2 lines: bbox + one cross road pair
		if (DL_SIZE(infileList) < 2){
			fprintf(stderr, "%s: Error empty or incomplete input file.\n", prog_name);
			fprintf (stderr, "Please see input file format in help with: %s --help\n", prog_name);
			retCode = ztMissFormatFile;
			goto cleanup;
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

			retCode = result;
			goto cleanup;
		}

		if (wktBboxFilePtr){

			formatBboxWKT(&bboxWktStr, &bbox);
			insertNextDL (bboxWktDL, DL_TAIL(bboxWktDL), bboxWktStr);
		}

		/* get cross road strings, parse them && stuff'em in a list */
		xrdsList = (DL_LIST *) malloc(sizeof(DL_LIST));
		if (xrdsList == NULL){
			fprintf(stderr, "%s: Error allocating memory.\n", prog_name);
			retCode = ztMemoryAllocate;
			goto cleanup;
		}

		initialDL (xrdsList, zapXrds, NULL);

		// point at second line - this is the INPUT FILE list
		elem = DL_NEXT(elem);
		while (elem) {

			lineInfo = (LINE_INFO*) elem->data;

			xrds = initialXrds(NULL, NULL); /* no street names yet */
			if ( ! xrds ){
				fprintf(stderr, "%s: Error failed initialXrds()!\n", prog_name);
				goto cleanup;
			}

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

		result = curlGetXrdsDL (xrdsList, &bbox, url);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error failed curlGetXrdsDL() !!!\n", prog_name);
			fprintf(stderr, "See FIRST error above ^^^^  exiting\n\n");
			retCode = result;
			goto cleanup;
		}

		// append this file list to session list
		result = appendToDL (xrdsSessionDL, xrdsList);
		if (result != ztSuccess){
			fprintf(stderr, "%s: Error failed appendToDL() !!!\n", prog_name);
			fprintf(stderr, "See FIRST error above ^^^^  exiting\n\n");
			retCode = result;
			goto cleanup;
		}

		destroyDL (xrdsList);
		free (xrdsList);

		argvPtr++; // next input file?

	} // end  while (*argvPtr)


	if (outputFilePtr) /* still show result in terminal */
		writeDL (NULL, xrdsSessionDL, writeXrds);

	writeDL (outputFilePtr, xrdsSessionDL, writeXrds);

	if (wktFileName && wktFilePtr){

		wktDL = (DL_LIST *) malloc (sizeof(DL_LIST));
		initialDL(wktDL, zapString, NULL);
		xrds2WKT_DL (wktDL, xrdsSessionDL);
		writeDL (wktFilePtr, wktDL, writeString2FP);

		// should destroy list and free memory
		fprintf (stdout, "Wrote Well Known Text to file: %s\n", wktFileNameExt);
		fclose (wktFilePtr);

	}

	if (outputFilePtr) {

		fprintf (stdout, "Wrote output to file: %s\n", outputFileName);
		fclose (outputFilePtr);
	}

	if (wktMidGpsName && wktMidGpsFilePtr){

		mgWktList = (DL_LIST *) malloc(sizeof(DL_LIST));
		if ( ! mgWktList ){
			fprintf(stderr, "%s: Error allocating memory for mgWktList.\n",
					prog_name);
			return ztMemoryAllocate;
		}
		initialDL (mgWktList, zapString, NULL);

		midGps2WKT_DL (mgWktList, xrdsSessionDL);

		writeDL (wktMidGpsFilePtr, mgWktList, writeString2FP);

		fprintf (stdout, "Wrote Mid-Point GPS Well Known Text to file: %s\n",
				     wktMidGpsName);
		fclose (wktMidGpsFilePtr);
	}

	if (wktBboxFilePtr){

		writeDL (wktBboxFilePtr, bboxWktDL, writeString2FP);
		// should destroy list and free memory too now
		fclose (wktBboxFilePtr);

		fprintf (stdout, "Wrote Bounding Box Polygon Well Known Text to file: %s\n",
				     wktBboxName);
	}

	if (rawDataFP) {

		fprintf (stdout, "Wrote raw data to file: %s\n", rawDataFileName);
		fclose (rawDataFP);
	}

	closeSession(); /* close curl session */

cleanup:
	if (home) {
		free(home);
		home = NULL;
	}

	if (outputFileName) {
		free(outputFileName);
		outputFileName = NULL;
	}
	if (service_url) {
		free(service_url);
		service_url = NULL;
	}
	if (myString) {
		free(myString);
		myString = NULL;
	}
	if (url) {
		curl_url_cleanup(url);
		url = NULL;
	}

	return retCode;

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
		wktStrArray = (char **) malloc ((sizeof(char *)) * (xrds->nodesNum + 1));
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

/* midGps2WKT_DL(): function formats midGps member into Well Known Text
 * from xrdsList and stores formatted string into destList. Caller initials destList.
 */
int midGps2WKT_DL (DL_LIST *destList, DL_LIST *xrdsList){

	DL_ELEM	*elem;
	XROADS	*xrds;
	char			*wktString;
	char			*wktMaker = "wkt;";

	ASSERTARGS (destList && xrdsList);

	if (DL_SIZE(xrdsList) == 0)

		return ztListEmpty;

	// start the list with wkt maker
	insertNextDL (destList, DL_TAIL(destList), wktMaker);

	elem = DL_HEAD(xrdsList);
	while(elem){

		xrds = (XROADS *) elem->data;

		if (xrds->nodesNum){

			wktString = gps2WKT (xrds->midGps);
			if ( ! wktString ){

				fprintf(stderr, "midGps2WKT_DL(): Error retuned from gps2WKT(). Exit.\n");
				return ztUnknownError;
			}

			insertNextDL (destList, DL_TAIL(destList), wktString);
		}

		elem = DL_NEXT(elem);

	} // end while(elem)


	return ztSuccess;
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

		newXrds = initialXrds(NULL, NULL);
		if ( ! newXrds ){
			fprintf(stderr, "%s : appendToDL(): Error failed initialXrds().\n", prog_name);
			return ztMemoryAllocate;
		}

		result = cpyXrds(newXrds, srcXrds);
		if (result != ztSuccess){
			fprintf(stderr, "%s : appendToDL(): Failed call to cpyXrds().\n", prog_name);
			return result;
		}

		result = insertNextDL (dest, DL_TAIL(dest), newXrds);
		if (result != ztSuccess){
			fprintf(stderr, "%s : appendToDL(): Error returned from insertNextDL().\n", prog_name);
			return result;
		}

		srcElem = DL_NEXT(srcElem);

	}

	return ztSuccess;
}

int curlGetXrdsDL (DL_LIST *xrdsDL, BBOX *bbox, CURLU *srvrURL){

	DL_ELEM		*elem;
	XROADS		*xrds;
	int				result;

	//do not allow nulls
	ASSERTARGS(xrdsDL && bbox && srvrURL);

	if(DL_SIZE(xrdsDL) == 0) // not even a warning

		return ztSuccess;

	/* one curl easy_handle being reused for the whole list. */
	CURL	*myCurlHandle =  initialQuery (srvrURL);
	if ( ! myCurlHandle){
		fprintf(stderr, "curlGetXrdsDL(): Error returned from initialQuery().\n");
		return ztGotNull;
	}

	elem = DL_HEAD(xrdsDL);
	while(elem){

		xrds = (XROADS *) elem->data;

		/* getXrdsGps() fills GPS members in xrds structure */
		result = getXrdsGps (xrds, bbox, srvrURL, myCurlHandle);
		if (result != ztSuccess){
			fprintf(stderr, "curlGetXrdsDL(): Error returned from getXrdsGps() function\n\n");
			return result;
		}
//printXrds(xrds);

		elem = DL_NEXT(elem);

	} //end while(elem)

	easyCleanup (myCurlHandle);

	return ztSuccess;

} // END curlGetXrdsDL()


