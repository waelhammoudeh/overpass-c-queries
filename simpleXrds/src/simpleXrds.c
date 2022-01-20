/*
 * simpleXrds.c
 *
 *  Created on: Jan 19, 2022
 *      Author: wael
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "overpass-c.h"
#include "curl_func.h"
#include "op_string.h"

FILE *rawDataFP = NULL;

int main(int argc, char* const argv[]) {

	/* three things to provide: street names, url to overpass server and bounding box */

	char		*firstRoad = "East Camelback Road",
				*secondRoad = "North 24Th Street";		/* or "North 32nd Street"; */
	char		*srvrURL = "http://localhost/api/interpreter";
	char		*bboxString = "33.5023615, -112.0345831, 33.5164598, -112.0038128";

	int			result;
	char			*bboxPtr;
	BBOX		*bbox;
	XROADS	*xrds;
	char			*queryString;

	CURLU 		*url;		/* enables us to use curl parser for URL */
	CURL		*myCurlHandle;
	MEMORY_STRUCT response;

	/* initial XROADS structure; allocates memory and fills names */
	xrds = initialXrds(firstRoad, secondRoad);
	if ( ! xrds ){
		fprintf(stderr, "Error returned from initialXrds()! Exiting.\n");
		return -1;
	}

	/* fill bbox structure, we allocate memory for bbox */
	bbox = (BBOX *) malloc(sizeof(BBOX));
	if ( ! bbox ){
		fprintf(stderr, "Error allocating memory for bbox. Exiting.\n");
		return -1;
	}

	bboxPtr = strdup(bboxString);

	result = parseBbox(bbox, bboxPtr);
	if ( result != 0 ){
		fprintf(stderr, "Error returned from parseBbox()! Exiting.\n");
		return -1;
	}

	queryString = xrdsFillTemplate (xrds, bbox);
	if ( ! queryString ){
		fprintf(stderr, "Error returned from xrdsFillTemplate()! Exiting.\n");
		return -1;
	}

	/* initial curl session */
	initialSession();

	url = initialURL (srvrURL);
	if (  ! url ){
		fprintf(stderr, "Error returned from initialURL() function.\n");
		closeSession();
		return -1;
	}

	myCurlHandle =  initialQuery (url);
	if ( ! myCurlHandle){
		fprintf(stderr, "Error returned from initialQuery().\n");
		urlCleanup(url);
		closeSession();
		return -1;
	}

	result = performQuery (&response, queryString, url, myCurlHandle);
	if ( result != 0 ){
		fprintf(stderr, "Error returned from performQuery()! Exiting.\n");
		urlCleanup(url);
		easyCleanup(myCurlHandle);
		closeSession();
		return -1;
	}

	/* your raw data now is at response.memory, parse it to fill GPS */
	result = parseCurlXrdsData (xrds, &response);
	if ( result != 0 ){
		fprintf(stderr, "Error returned from parseCurlXrdsData()! Exiting.\n");
		urlCleanup(url);
		easyCleanup(myCurlHandle);
		closeSession();
		return -1;
	}

	fprintf(stdout, "The server raw data was:\n");
	fprintf(stdout, "%s\n\n", response.memory);

	printXrds(xrds);

	urlCleanup(url);
	easyCleanup(myCurlHandle);
	closeSession();

	return 0;
}

