/*
 * overpass-c.c
 *
 *  Created on: Mar 10, 2021
 *      Author: wael
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "overpass-c.h"
#include "util.h"
#include "ztError.h"
#include "curl_func.h"
#include "op_string.h"
#include "fileio.h"

/* Note: with (extern FILE *rawDataFP;) declaration in overpass.h
 * user can declare as global variable, and open a file - received data will
 * be written to that file. Declaration is in the overpass.h header file with:
 * and code in in curlGetXrdsGPS() function below. ***/

/* xrdsFillTemplate(): fills query template given firstRD + secondRD && bbox
 * Allocates required memory for the string - this is the query part of URL.
 * Returns char* for the query string or NULL on error.
************************************************************************ */

char *xrdsFillTemplate (XROADS *xrds, BBOX *bbox){

	char			*queryTemplate =
						"[out:csv(::lat,::lon,::count)]"
						"[bbox:%10.7f,%10.7f,%10.7f,%10.7f];"
						"(way['highway'!='service']['name'~'%s', i];>;)->.outNS;"
						"(way['highway'!='service']['name'~'%s', i];>;)->.outEW;"
						"node.outNS.outEW;"
						//"out; out count;&";
						"out; out count;";   // do not include the '&' at the end
						// used single NOT double quotes above in RegExp!

	char			tmpBuf[LONG_LINE * 2] = {0}; // large buffer
	char			*retValue = NULL;
	char			*cleanFirstRD, *cleanSecondRD;
	int			result;

	ASSERTARGS (xrds && bbox);

	// the two roads members should be set in the structure
	ASSERTARGS(xrds->firstRD && xrds->secondRD);

	// remove leading and trailing white space
	cleanFirstRD = strdup (xrds->firstRD);
	cleanSecondRD = strdup (xrds->secondRD);

	removeSpaces (&cleanFirstRD);
	removeSpaces (&cleanSecondRD);

	if ( ! isBbox(bbox)){

		printf("xrdsFillTemplate(): Error isBbox() return FALSE! "
				   "Invalid BOUNDING BOX.\n");
		//return ztInvalidArg;
		//FIXME xrdsFillTemplate() should return integer TODO

		return retValue; // set to NULL -
	}

	/* Note for snprintf(): the return type is of "size_t" AND if the return
	 * value is (LONG_LINE * 2) or more that means that the output was
	 * truncated - partial copy is an error.
	*******************************************************************/
	result = (int) snprintf (tmpBuf, (LONG_LINE * 2), queryTemplate,
					                      bbox->sw.gps.latitude,bbox->sw.gps.longitude,
										  bbox->ne.gps.latitude, bbox->ne.gps.longitude,
										  cleanFirstRD, cleanSecondRD);

	if (result > (LONG_LINE * 2) ){

		printf ("xrdsFillTemplate(): Error, QUERY buffer - tmpBuf size of "
				"(LONG_LINE * 2) is TOO SMALL.\n");
//return ztSmallBuf; // I return char* FIXME make return integer - else set global error
		return NULL;
	}

	retValue = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
	if (retValue == NULL){
		printf ("xrdsFillTemplate(): Error allocating memory.\n");
		return retValue;
	}

	strcpy (retValue, tmpBuf);

	return retValue;

} //END xrdsFillTemplate()

/* isOkResponse(): remote server response is okay if first line in the
 * response matches header. - see comment in curlGetXrdsGPS() func
 * Question? : I am hoping this tells me the query was understood by
 * the server; the query syntax was correct.
 */

int isOkResponse (char *response, char *header){

	char		*firstLine, *memStart, *chPtr;
	int		chCount, retCode;

	ASSERTARGS (response && header);

	memStart = response;
	chPtr = strchr (memStart, '\n');

	chCount = (int) (chPtr - memStart);

	firstLine = (char *) malloc (sizeof(char) *  chCount + 1);
	if ( ! firstLine ){

		printf("isOkResponse(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	strncpy (firstLine, memStart, chCount);
	firstLine[chCount] = '\0';

	if (strcmp (firstLine, header) == 0){

		retCode = ztSuccess;
	}
	else {

		printf ("isOkResponse(): Error: Not a valid response. Server may responded "
				    "with an error message! The server response was:\n\n");
		printf (" Start server response below >>>>:\n\n");
		printf ("%s\n\n", response);
		printf (" >>>> End server response This line is NOT included.\n\n");
		retCode = ztInvalidResponse;
	}

	free (firstLine);
	return retCode;
}

/* isBbox() : function checks that the structure pointed to by bbox holds
 * a valid bounding box as defined by Overpass API; that is the south-west
 * POINT is really south-west relative to north-east POINT.
 * Return: function returns TRUE if structure holds a valid bounding box and
 * FALSE otherwise.
 * Note: this replaces the never implemented checkBbox().
 ***************************************************************************/
int isBbox(BBOX *bbox){

	ASSERTARGS (bbox);

	// maybe a lot of noise!?
	if (bbox->sw.gps.longitude > bbox->ne.gps.longitude)
		printf ("isBbox(): invalid member is: LONGITUDE.\n");

	if (bbox->sw.gps.latitude > bbox->ne.gps.latitude)
		printf ("isBbox(): invalid member is: LATITUDE.\n");

	if ( (bbox->sw.gps.longitude < bbox->ne.gps.longitude) &&
		  (bbox->sw.gps.latitude < bbox->ne.gps.latitude) )

		return TRUE;

	return FALSE;

}

int namesFillTemplate(char **dst, BBOX *bbox){

	char		*qryTemplate = "[out:csv('name' ;false)];"
										  "way(%10.7f ,%10.7f ,%10.7f ,%10.7f)"
										  "[highway]; out;";
	int		result;
	char		tmpBuf[LONG_LINE * 2] = {0}; // large buffer


	ASSERTARGS (dst && bbox);

	*dst = NULL; // set value to return to NULL

	if ( ! isBbox(bbox) ){

		printf("namesFillTemplate(): Error isBbox() return FALSE! "
				   "Invalid BOUNDING BOX.\n");
		return ztInvalidArg;
	}

	result = snprintf (tmpBuf,  (LONG_LINE * 2), qryTemplate,
				  	  	  	  	  bbox->sw.gps.latitude,bbox->sw.gps.longitude,
								  bbox->ne.gps.latitude, bbox->ne.gps.longitude);

	if (result > (LONG_LINE * 2) ){

		printf("namesFillTemplate(): Error; tmpBuf size of "
				   "(LONG_LINE * 2) is TOO SMALL.\n");
		return ztSmallBuf;
	}

	*dst = (char *) malloc ((strlen(tmpBuf) + 1) * sizeof(char));
	if (*dst == NULL){
		printf ("namesFillTemplate(): Error allocating memory.\n");
		return ztMemoryAllocate;
	}

	strcpy (*dst, tmpBuf);

	return ztSuccess;
}

/* cpyXrds(): copies src XROADS structure to dest. client allocates dest.
 *********************************************************************/
int cpyXrds (XROADS *dest, XROADS *src){

	int	num;

	/* no NULL allowed here */
	ASSERTARGS (dest && src);

	if (src->firstRD)
		dest->firstRD = strdup(src->firstRD);

	if (src->secondRD)
		dest->secondRD = strdup(src->secondRD);

	memcpy (dest->point, src->point, sizeof(POINT));

	dest->nodesNum = src->nodesNum;

	for (num = 0; num < src->nodesNum; num++)

		memcpy(dest->nodesGPS[num], src->nodesGPS[num], sizeof(GPS));


	memcpy(dest->midGps, src->midGps, sizeof(GPS));

	return ztSuccess;
}

/* initialXrds(): allocates memory for structure, sets firstRD and secondRD
 * members if passed to function.
 ************************************************************************/
XROADS * initialXrds (char *firstRd, char *secondRd){

	XROADS	*newXrd = NULL;

	newXrd = (XROADS *) malloc(sizeof(XROADS));
	if (! newXrd){
		fprintf(stderr, "initialXrds(): Error failed malloc().\n");
		return newXrd;
	}

	memset(newXrd, 0, sizeof(XROADS));

	newXrd->point = (POINT *) malloc(sizeof(POINT));

	for (int num = 0; num < MAX_NODES; num++){

		newXrd->nodesGPS[num] = (GPS *) malloc(sizeof(GPS));
		if ( ! newXrd->nodesGPS[num] ){
			fprintf(stderr, "initialXrds(): Error allocating memory.\n");
			newXrd = NULL;
			return newXrd;
		}
	}

	newXrd->midGps = (GPS *) malloc(sizeof(GPS));
	if ( ! newXrd->midGps){
		fprintf(stderr, "initialXrds(): Error allocating memory.\n");
		newXrd = NULL;
		return newXrd;
	}

	if (firstRd && secondRd){
		newXrd->firstRD = strdup(firstRd);
		newXrd->secondRD = strdup(secondRd);
	}

	return newXrd;
}

/* zapXrds(): the opposite of initialXrds() */
void zapXrds (void **xrds){

	XROADS 	*pxrds;

	ASSERTARGS (xrds);

	pxrds = (XROADS *) *xrds;

	if (! pxrds){
		fprintf(stderr, "zapXrds(): Hey you;;;; passed me NULL!\n");
		return;
	}

	if (pxrds->firstRD)
		free(pxrds->firstRD);

	if (pxrds->secondRD)
		free(pxrds->secondRD);

	for (int num = 0; num < MAX_NODES; num++) {

		if (pxrds->nodesGPS[num]) {
			free(pxrds->nodesGPS[num]);

		}
	}
	memset(pxrds, 0, sizeof(XROADS));
	free(pxrds);

	return;

}

int getXrdsGps (XROADS *xrds, BBOX *bbox, CURLU *srvrURL, CURL *curlHandle){

	MEMORY_STRUCT		myDataStruct;
	char		*query;
	int		result;
	char		*hdrSignature = "@lat	@lon	@count"; // no "\n" included

	ASSERTARGS (xrds && bbox && srvrURL && curlHandle);

	query = xrdsFillTemplate (xrds, bbox);
	if (query == NULL){

		printf("getXrdsGps(): Error returned from xrdsFillTemplate().\n");
		return ztMemoryAllocate;
	}

	result = performQuery (&myDataStruct, query, srvrURL, curlHandle);
	if (result != ztSuccess){

		fprintf (stderr, "getXrdsGps(): Error returned from performQuery().\n");
		return result;

	}

	/* client can declare a global variable "rawDataFP" then open
	 * file for writing with "rawDataFP" set as the file pointer,
	 * the received data then will be written to that file as received.
	 ******************************************************************************/

	/* write received data to client opened file if rawDataFP is set */
	if (rawDataFP) {

		fprintf(rawDataFP, "Data for cross roads: [ %s && %s ]\n\n",
				xrds->firstRD, xrds->secondRD);

		fprintf(rawDataFP, "%s", myDataStruct.memory);
		fprintf(rawDataFP,
				"\n ++++++++++++++++++++++++++++++++++++++++++++++++\n\n");
		fflush(rawDataFP);

	}

	/* the above function call and a successful result test ONLY tell us that
	 * we received some response from the server. Is it what we want? Or
	 * is it an error message - bad query, busy or down server, maybe we
	 * got band by the server or ...? For Overpass server we get an HTML
	 * page in case of an error, where in a successful response the first line
	 * will be the HEADER we requested in our query by the following line from
	 * our query template:
	 * "[out:csv(::lat,::lon,::count)]"
	 * The above line use the default of "true or yes" for the header, to
	 * turn off header listing you have to change the above line to:
	 * "[out:csv(::lat,::lon,::count ; false)]"
	 *
	 * So with this out of the way, we examine the first line and compare
	 * it to the expected header before we proceed, an okay response is
	 * one that has a header which matches our expected header.
	 ************************************************************************/

	result = isOkResponse(myDataStruct.memory, hdrSignature);
	if (result != ztSuccess) {
		fprintf(stderr, "getXrdsGPS(): Error returned from isOkResponse()!\n"
				" The error was: %s\n\n", code2Msg(result));
		return result;
	}

	result = parseCurlXrdsData(xrds, &myDataStruct);
	if (result != ztSuccess) {
		printf("getXrdsGps(): Error returned from parseCurlXrdsData()!\n"
				" The error was: %s\n\n", code2Msg(result));
		return result;
	}

	return ztSuccess;
}
