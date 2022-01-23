#ifndef XRDS2GPS_H_
#define XRDS2GPS_H_

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "fileio.h" // this includes dList.h on top
#include "overpass-c.h"
#include "ztError.h"

// functions prototype
void shortUsage (FILE *toFP, ztExitCodeType exitCode);

void printHelp(FILE *toStream);

int curlGetXrdsDL (DL_LIST *xrdsDL, BBOX *bbox, CURLU *url);

int getXrdsDL(DL_LIST *xrdsDL, BBOX *bbox, char *server, char *outDir);

int xrds2WKT_DL (DL_LIST *dstDL, DL_LIST *srcDL);

int midGps2WKT_DL (DL_LIST *destList, DL_LIST *xrdsList);

/* it is a mistake to tag a slash at the end of the URL */

/* overpass server, use only one! */
//#define	SERVICE_URL		"http://localhost/api/interpreter"
#define	SERVICE_URL		"http://127.0.0.1/api/interpreter"
//#define	SERVICE_URL		"http://www.overpass-api.de/api/interpreter"
//#define	SERVICE_URL		"http://yafa.local/api/interpreter"
//#define	SERVICE_URL		"http://lazyant.local/api/interpreter"
//#define	SERVICE_URL		"http://lazyant/api/interpreter"

#define	NUM_TRIES	3

// exported globals
extern const char *prog_name;


#endif /*  XRDS2GPS_H_ */

