/*
 * op_string.h
 *
 *  Created on: Mar 31, 2021
 *      Author: wael
 */

#ifndef OP_STRING_H_
#define OP_STRING_H_

#include "overpass-c.h"
#include "dList.h"

int parseBbox(BBOX *bbox, char *string);

int xrdsParseNames(XROADS *dest, char *str);

int parseCurlXrdsData (XROADS *xrds, void *data);

int parseWgetXrdsFile (XROADS *dst, void *filename);

int parseXrdsResult (XROADS *dstXrds, DL_LIST *srcDL);

int response2LineDL (DL_LIST *dstDL, char *response);

int parseGPS (GPS *dst, char *str);

void printXrds (XROADS *xrds);

void writeXrds (FILE *file, void *data);

char *gps2WKT (GPS *gps);

int xrds2WKT (char **dst, XROADS *xrds);

void writeString2FP (FILE *to, void *str);

void printXrds(XROADS *xrds);

#endif /* OP_STRING_H_ */
