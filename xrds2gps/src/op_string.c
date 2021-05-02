/* op_string.c : this file has overpass string handling functions.
 * I have some string functions in the util.c file. Functions here are mostly
 * for parsing string data related to overpass.
 ***********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "overpass-c.h"
#include "dList.h"
#include "fileio.h"
#include "util.h"
#include "ztError.h"

#include "curl_func.h"
#include "op_string.h"

/* Functions parseBbox() and xrdsParseNames() are used to parse input file */

/* parseBbox(): parses string pointed to by string, assumed to be in the
 * following format with comma as delimiter:
 *  south-west latitude, south-west longitude, north-east latitude, north-east longitude
 *  example: 33.53097, -112.07400, 33.56055, -112.0567345
 *  It then fills BBOX structure pointed to by bbox.
 *  Parameters:
 *  	bbox: pointer to BBOX structure.
 *  	string: a character pointer to string to parse.
 *  Return: ztSuccess on success.
 *  Checked run time errors the function may return:
 *  ztBadLineZI, ztGotNull, ztInvalidToken, ztDisallowedChar
 *
 *  ****************************************************/
int parseBbox(BBOX *bbox, char *string){

	char			*delim = ",";
	char			*token;
	char			*allowed = "0123456789.-+"; //digits, period, - and + signs
	double	numDbl;
	char			*endPtr;
	int			i;
	char			*SPACESET = "\040\t";  // space and tab
	char			*chPtr;

	// do not allow null pointers
	ASSERTARGS (bbox && string);

	// line should have exactly three commas
	i = 0;
	for (chPtr =  string; (chPtr = strchr(chPtr, ',')) != NULL; chPtr++, i++);

	if (i != 3){
		printf("parseBbox(): Error bad formated line. Incorrect number of commas!\n");
		printf ("   < %s >\n", string);
		return ztBadLineZI;
	}

	/* get bbox members, that is 4 doubles delimited by comma */
	for (i = 0; i < 4; i++){

		if(i == 0)
			token = strtok(string, delim);
		else
			token = strtok(NULL, delim);

		if (token == NULL) {
			printf("parseBbox(): Error; could not get token number %d! NULL.\n", i+1);
			return ztGotNull;
		}

		// is token ALL spaces?
		if(strspn(token, SPACESET) == strlen(token)){
			printf("parseBbox(): Error; ALL spaces token number %d! \n", i+1);
			return ztInvalidToken;

		}

		removeSpaces(&token);

		if(strspn(token, allowed) != strlen(token)){ // disallowed char found
			printf("parseBbox(): Disallowed character in token number %d: [%s]\n",
					    i+1, token);
			return ztDisallowedChar;
		}

		numDbl = (double) strtod (token, &endPtr);
		if (*endPtr != '\0') {
			printf("parseBbox(): Error invalid token for double: <%s>.\n", token);
			return ztInvalidToken;
		}

		/* check range for PHOENIX, ARIZONA */
		switch (i){
			case 1:
			case 3:

			if (! LONGITUDE_OK (numDbl)){
				printf("parseBbox(): Error; invalid Phoenix longitude. <%f>\n", numDbl);
				return ztInvalidToken;
			}
			break;

			case 0:
			case 2:

			if (! LATITUDE_OK(numDbl)){
				printf("parseBbox(): Error; invalid Phoenix latitude. <%f>\n", numDbl);
				return ztInvalidToken;
			}
			break;

			default:
			break;

		}  /* end switch to check range */

		switch (i){  /* assign */

		case 0:
			bbox->sw.latitude = numDbl;
			break;

		case 1:
			bbox->sw.longitude = numDbl;
			break;

		case 2:
			bbox->ne.latitude= numDbl;
			break;

		case 3:
			bbox->ne.longitude = numDbl;
			break;

		default:
			break;

		}  /* end switch to assign */

	} /* end for (...) get gps bbox */

	// we might need to check the rest of the line!

	return ztSuccess;

} // END parseBbox()

/* xrdsParseNames(): parses string into 2 cross road names,  comma as delimiter,
 * road names are placed into dest [XROADS struct] firstRd and secondRd members.
 * returns integer as follows:
 *		ztSuccess: on success
 *		ztParseError: missing comma delimiter or missing token
 *		ztDisallowedChar: found disallowed character within a token
 **********************************************************************/
int xrdsParseNames(XROADS *dest, char *str){

	char			*delim = ",";
	char			*token1, *token2;
	char			*disallowed = "~!@#$%^&*()_+./\\|\":`<>[{]}"; //disallowed char set
	int			COMMA = ',';
	char			*ptr4COMMA = NULL;

	// make sure we have a comma delimiter -
	// we can also check its position; error if it is first or last character
	ptr4COMMA = strchr (str, COMMA);
	if (ptr4COMMA == NULL){
		printf ("xrdsParseNames(): Error line is missing the comma delimiter!\n");
		return ztParseError;
	}

	token1 = strtok(str, delim);
	token2 = strtok(NULL, delim);

	if ( (token1 == NULL) || (token2 ==NULL) ){
		printf ("xrdsParseNames(): Error got NULL for token1 or token2!\n");
		return ztParseError;
	}

	removeSpaces(&token1);
	removeSpaces(&token2);

	if(strcspn(token1, disallowed) != strlen(token1)){
		printf("xrdsParseNames(): token1 <%s> has disallowed character. *****\n", token1);
		return ztDisallowedChar;
	}
	if(strcspn(token2, disallowed) != strlen(token2)){
		printf("xrdsParseNames(): token2 <%s> has disallowed character. *****\n", token2);
		return ztDisallowedChar;
	}

	dest->firstRD = strdup(token1);
	dest->secondRD = strdup(token2);

	return ztSuccess;
}


int parseGPS (GPS_PT *dst, char *str){
/* parse GPS point latitude and longitude members (overpass result line)
 * <	33.5605235		-112.0652852	> store result in dst members
 * dst is pointer to GPS_PT structure in parseGPS2()
 * was XROADS pointer in parseGPS() - earlier function */
	char			*myStr;
	char			*delim = "\040\t";
	char			*token1, *token2;
	char			*allowed = "0123456789.-"; //digits, period and minus sign
	double	numLat, numLng;
	char			*endPtr;

	// do not allow null pointers
	ASSERTARGS (dst && str);

	myStr = strdup(str);

	token1 = strtok(myStr, delim);
	token2 = strtok(NULL, delim);

	if ((token1 == NULL ) || (token2 == NULL )) {
		printf("parseGPS2(): Error; could not a get token! One of two is NULL.\n");
			return ztGotNull;
	}

	// removeSpaces(&token1); wrong! strtok() does this already

	if(strspn(token1, allowed) != strlen(token1)){ // disallowed char found
		printf("parseGPS2(): Disallowed character in token number 1.\n");
		return ztDisallowedChar;
	}

	numLat = (double) strtod (token1, &endPtr);
	if (*endPtr != '\0') {
		printf("parseGPS2(): Error invalid token for double: <%s>.\n", token1);
		return ztInvalidToken;
	}

	if (! LATITUDE_OK(numLat)){
		printf("parseGPS2(): Error; invalid Phoenix latitude. <%f>\n", numLat);
		return ztInvalidToken;
	}

	// removeSpaces(&token2);

	if(strspn(token2, allowed) != strlen(token2)){ // disallowed char found
		printf("parseGPS2(): Disallowed character in token number 2.\n");
		return ztDisallowedChar;
	}

	numLng = (double) strtod (token2, &endPtr);
	if (*endPtr != '\0') {
		printf("parseGPS2(): Error invalid token for double: <%s>.\n", token2);
		return ztInvalidToken;
	}

	if (! LONGITUDE_OK (numLng)){
		printf("parseGPS2(): Error; invalid Phoenix longitude. <%f>\n", numLng);
		return ztInvalidToken;
	}

	// assign values
	dst->longitude = numLng;
	dst->latitude = numLat;

	return ztSuccess;
}



int parseCurlXrdsData (XROADS *xrds, void *data){

	MEMORY_STRUCT *theData;
	char		*str;
	char		*linefeed = "\n";
	char		*ptr;

	int				lineNum = 0;
	DL_LIST		linesList; // will insert line by line into the list
	LINE_INFO	*lineInfo;
	int				result;

	ASSERTARGS (xrds && data);

	theData = (MEMORY_STRUCT *) data;

	// initial the list
	initialDL (&linesList, NULL, NULL);

	str = strdup(theData->memory);

	ptr = strtok(str, linefeed);
	if (ptr == NULL){
		printf("parseCurlXrdsData(): Error first ptr is NULL.\n");
		return ztGotNull;
	}

	while (ptr){

		// allocate memory for lineInfo
		lineInfo = (LINE_INFO *) malloc (sizeof(LINE_INFO));
		if ( ! lineInfo){

			printf("parseCurlXrdsData(): Error allocating memory for lineInfo!\n");
			return ztMemoryAllocate;
		}
		// set members
		lineInfo->originalNum = lineNum;
		lineInfo->string = ptr;

		// insert into list -
		result = insertNextDL (&linesList, DL_TAIL(&linesList), (void *) lineInfo);
		if (result != ztSuccess){
			printf("parseCurlXrdsData(): Error returned by insertNextDL().\n");
			return result;
		}

		lineNum++;
		ptr = strtok (NULL, linefeed);

	} // end while(ptr) or parse lines.

	result = parseXrdsResult (xrds, &linesList);
	if (result != ztSuccess){

		printf ("parseCurlXrdsData(): Error returned by parseOverpassResult().\n");
		destroyDL(&linesList);
		return result;
	}

	 printf ("parseCurlXrdsData(): Printing CURRENT XROADS structure:\n");
	 printXrds (xrds);

	destroyDL(&linesList);

	return ztSuccess;
}

/* parseWgetXrdsFile (): parses disk file downloaded by wget call to overpass
 * server. Fills dst (XROADS struct) members with parsed values for structure
 * members point (with longitude and latitude) also the nodesFound member.
 *
 * The second parameter is of type "void" because this function is called with
 * a function pointer, the same as parseCurlXrdsData(). The two functions
 * need to have the SAME exact prototype
 ****************************************************************************/
int parseWgetXrdsFile (XROADS *dst, void *filename){

	DL_LIST		*outFileDL;
	int				result;

	ASSERTARGS (dst && filename); // do not allow nulls

	outFileDL = (DL_LIST *) malloc(sizeof(DL_LIST));
	if (outFileDL == NULL){
		printf("parseWgetXrdsFile(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}
	initialDL (outFileDL, zapLineInfo, NULL);

	result = file2List(outFileDL, (char *) filename);
	if (result != ztSuccess){
		printf("parseWgetXrdsFile(): Error returned by file2List()!\n");
		return result;
	}

	result = parseXrdsResult (dst, outFileDL);
	if (result != ztSuccess){

		printf ("parseWgetXrdsFile(): Error returned by parseOverpassResult().\n");
		destroyDL(outFileDL);
		free(outFileDL);
		return result;
	}

	destroyDL(outFileDL);
	free(outFileDL);
	return ztSuccess;

}

/* the srcDL is double linked list with the query result from overpass
 * server, with each line placed in an element within a LINE_INFO
 * structure; first line is the header we request in the query by:
 *  "[out:csv(::lat,::lon,::count)]"
 * and last line has the number of nodes found, it is zero if no common
 * nodes were found.
 */
int parseXrdsResult (XROADS *dstXrds, DL_LIST *srcDL){

	DL_ELEM		*elem;
	LINE_INFO	*lineInfo;
	int				numFound;
	char				*str;
	int				iCount, result;

	ASSERTARGS (dstXrds && srcDL);

	// set first nodesGPS pointer to NULL; change below when nodes found
	dstXrds->nodesGPS[0] = NULL;

	// last line has the number of returned nodes found
	elem = DL_TAIL(srcDL);
	lineInfo = DL_DATA(elem);

	str = (char *)lineInfo->string;
	sscanf(str, "%d", &numFound);

	// set nodesFound member of the XROADS structure.
	dstXrds->nodesFound = numFound;

	 if (numFound == 0) // Done; nothing left to do

		 return ztSuccess;

	 // found intersection(s), parse them all and stuff'em in GPS_PT array
	 /* allocate pointers in the array for the numFound plus one, last will
	  * be set to NULL.
	  */
	 for (iCount = 0; iCount < numFound; iCount++){

		 dstXrds->nodesGPS[iCount] = (GPS_PT *)  malloc(sizeof(GPS_PT));
		 if ( ! dstXrds->nodesGPS[iCount] ){
			 printf ("parseXrdsResult(): Error allocating memory.\n");
			 return ztMemoryAllocate;
		 }
	 }
	 dstXrds->nodesGPS[numFound] = NULL;

	 /* we have enough allocated pointers, each line starting at second line
	  * has a GPS point, point at the second line in the source list, fill 'em up */
	 elem = DL_HEAD(srcDL);
	 elem = DL_NEXT(elem); // second line

	 for (iCount = 0; iCount < numFound; iCount++, elem = DL_NEXT(elem)){

		 ASSERTARGS (elem);

		 lineInfo = DL_DATA(elem);
		 str = (char *)lineInfo->string;

		 result = parseGPS (dstXrds->nodesGPS[iCount], str);
		 if (result != ztSuccess) {
			 printf ("parseXrdsResult(): Error returned by parseGPS2().\n");
			 return result;
		 }

	 }

	 // set XRDOADS point member to first GPS found
	 dstXrds->point.longitude = dstXrds->nodesGPS[0]->longitude;
	 dstXrds->point.latitude= dstXrds->nodesGPS[0]->latitude;

	 return ztSuccess;
}
/* response2LineDL() : function parses overpass response - gets white space
 * clean tokens - for street for street names query
 * lines are placed in double linked list of pointers to CHARACTER STRINGS.
 * In parseCurlXrdsData() the pointer in the list is to LINE_INFO structure; to
 * match file2List() parameters!
 *******************************************************************************/
int response2LineDL (DL_LIST *dstDL, char *response){

	char		*str;
	char		*linefeed = "\n";
	char		*ptr;
	int		result;

	ASSERTARGS (dstDL && response);

	str = strdup(response); // get our own copy

	ptr = strtok(str, linefeed);
	if (ptr == NULL){
		printf("response2LineDL(): Error first ptr is NULL.\n");
		return ztGotNull;
	}

	while (ptr){

		// insert line into list -
		result = insertNextDL (dstDL, DL_TAIL(dstDL), (void *) ptr);
		if (result != ztSuccess){
			printf("response2LineDL(): Error returned by insertNextDL().\n");
			return result;
		}

		ptr = strtok (NULL, linefeed);

	} // end while(ptr)

	return ztSuccess;

}

void printXrds (XROADS *xrds){ // partial - half-assed job

	int	iCount;

	ASSERTARGS (xrds);

	printf ("[%s, %s]  (%10.7f, %10.7f) FOUND: %d nodes.\n",
			  xrds->firstRD, xrds->secondRD,
			  xrds->point.longitude, xrds->point.latitude,
			  xrds->nodesFound);
	printf("\tNodes Found Total [ %d ] :\n", xrds->nodesFound);

	for (iCount = 0; iCount < xrds->nodesFound; iCount++)

		printf("\t (%10.7f, %10.7f)\n",
				xrds->nodesGPS[iCount]->longitude,
				xrds->nodesGPS[iCount]->latitude);



	return;
}

/* formats GPS as Well Known Text POINT: "POINT ((-111.917714 33.407882))"
 * function allocates memory for buffer, no line feed is used. */
char *gps2WKT (GPS_PT *gps){

	char		*retPtr = NULL;
	int		bufSize = 36;

	ASSERTARGS (gps);

	retPtr = (char *) malloc(sizeof(char) * bufSize);
	if ( ! retPtr ){
		printf ("gps2WKT(): Error allocating memory.\n");
		return retPtr;
	}
	memset (retPtr, 0, bufSize);

	sprintf (retPtr, "\"POINT ((%10.7f %10.7f))\"", gps->longitude, gps->latitude);

	return retPtr;

}
