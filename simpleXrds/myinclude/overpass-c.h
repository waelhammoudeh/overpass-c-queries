/*
 * overpass-c.h
 *
 *  Created on: Mar 10, 2021
 *      Author: wael
 */

#ifndef OVERPASS_C_H_
#define OVERPASS_C_H_

#include <stdio.h>
#include "curl_func.h"

/* LONGITUDE_OK(i) and LATITUDE_OK(i) are both
 *  macros to validate longitude and latitude values in the
 * Phoenix, Arizona area - includes most of Maricopa county.
 *          *****xxxxx******xxxx******
 * change those values for your area.
 **********************************************************/
#define LONGITUDE_OK(i) (((i) > -113.0 && (i) < -111.0))
#define LATITUDE_OK(i) (((i) > 32.8 && (i) < 33.95))

/* exported variable for raw data file pointer, when set by client raw query
 * result from overpass server is written to that open file.
 *************************************************************************/
extern FILE *rawDataFP;

/* type definitions */
typedef struct GPS_ {

	double 	longitude,
					latitude;
} GPS;

typedef struct HB_ { // hundred block number

	int	ns, ew;

} HB;

// limit name length in POINT
#define POINT_NAME_LENGTH 48

typedef struct POINT_ {

	GPS		gps;
	HB		hb;
	char		name[POINT_NAME_LENGTH];

} POINT;

typedef struct BBOX_ {

	   POINT		sw, ne;
} BBOX;

/* limit number of GPS nodes we store */
#define MAX_NODES 8

typedef struct XROADS_ {

	char		*firstRD, *secondRD;
	POINT	*point;
	int		nodesNum;
	GPS		*nodesGPS[MAX_NODES];
	GPS		*midGps;

} XROADS;

#define MAX_SRC_LENGTH  65
#define MAX_ID_LENGTH  17

typedef struct RECT_DATA_ {

	char		srcFile[MAX_SRC_LENGTH];
	char		idRxC[MAX_ID_LENGTH];

} RECT_DATA;

typedef struct RECTANGLE_ {

	POINT		    nw, ne, se, sw;
	RECT_DATA	data;

}RECTANGLE;

/* functions prototypes */

XROADS * initialXrds (char *firstRd, char *secondRd);

int cpyXrds (XROADS *dest, XROADS *src);

void zapXrds (void **xrds);

char* xrdsFillTemplate (XROADS *xrds, BBOX *bbox);

int isOkResponse (char *response, char *header);

int namesFillTemplate(char **dst, BBOX *bbox);

int isBbox(BBOX *bbox);

int getXrdsGps (XROADS *xrds, BBOX *bbox, CURLU *srvrURL, CURL *myCurlHandle);

#endif /* OVERPASS_C_H_ */
