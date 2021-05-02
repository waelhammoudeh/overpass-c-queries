#ifndef XRDS2GPS_H_
#define XRDS2GPS_H_

#include <stdio.h>
#include <stdlib.h>
#include "fileio.h" // this includes dList.h on top
#include "overpass-c.h"

// functions prototype
int getXrdsDL(DL_LIST *xrdsDL, BBOX *bbox, char *server, char *outDir);

int curlGetXrdsDL(DL_LIST *xrdsDL, BBOX *bbox, char *server, char *outDir);

int writeXrdsDL_WKT (DL_LIST *xrdsDL, char *toDir);

/* it is a mistake to tag a slash at the end of the path */
#define	SERVICE_URL		"http://localhost/api/interpreter"
//#define	SERVICE_URL		"http://127.0.0.1/api/interpreter"
//#define	SERVICE_URL		"http://www.overpass-api.de/api/interpreter"
//#define	SERVICE_URL		"http://yafa.local/api/interpreter"
//#define	SERVICE_URL		"http://lazyant.local/api/interpreter"
//#define	SERVICE_URL		"http://lazyant/api/interpreter"

#define	NUM_TRIES	3

// DEBUG_QUERY define or undefine to save raw data from curl query to disk
#define DEBUG_QUERY
// #undef DEBUG_QUERY

// exported globals
extern const char *prog_name;
extern FILE	*rawDataFP;


#endif /*  XRDS2GPS_H_ */

