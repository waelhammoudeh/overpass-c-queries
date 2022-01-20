#include <stdio.h>
#include <string.h>
#include <curl/curl.h>

#include "curl_func.h"
#include "util.h"
#include "ztError.h"

// global variables
static  int     sessionFlag = 0; // global initial flag

/* initialSession(): checks libcurl version and calls curl_global_init()
 * then sets sessionFlag. Call this function first to use any other
 * functions here or libcurl functions. Call closeSession() when done.
 * MIN_CURL_VER is defined in curl_func.h header file.
****************************************************************************/
int initialSession(void){

	CURLcode	result;
	curl_version_info_data *verInfo; /* script or auto tools maybe?? ****/

	if (sessionFlag) // call me only once

		return ztSuccess;

	verInfo = curl_version_info(CURLVERSION_NOW);
	if (verInfo->version_num < MIN_CURL_VER){

		fprintf (stderr, "ERROR: Required \"libcurl\" minimum version is: 7.80.0. Aborting.\n");
		return ztInvalidUsage;
	}

	result = curl_global_init(CURL_GLOBAL_ALL);
	if (result != 0){
	    fprintf(stderr, "curl_global_init() failed: %s\n",
	            curl_easy_strerror(result));
	    return result;

	}

	sessionFlag = 1;

	return ztSuccess;
}

void closeSession(void){

	if (sessionFlag == 0)

		return;

	/* REMOVED: curl_easy_cleanup() is done per handle 12/19/2021 */

	/* we're done with libcurl, so clean it up */
	curl_global_cleanup();

	sessionFlag = 0;

	return;
}

/* WriteMemoryCallback(): call back function copied from curl source examples.
 **************************************************************************** */
static size_t WriteMemoryCallback (void *contents, size_t size,
                                            size_t nmemb, void *userp) {

  size_t realsize = size * nmemb;
  MEMORY_STRUCT *mem = (MEMORY_STRUCT *) userp;

  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    /* out of memory! */
    fprintf(stderr, "WriteMemoryCallback(): Error not enough memory "
    		"(realloc returned NULL)\n");
    return 0;
  }

  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

/* initialURL(): calls curl_url() and sets server url to let libcurl do URL
 * parsing for us. Parameter server maybe NULL, should be at least scheme
 * and server name. Caller must call urlCleanup(retValue) when done.
 */
CURLU * initialURL (char *server){

	CURLU *retValue = NULL;
	CURLUcode result;


	retValue = curl_url();

	if (retValue == NULL)

		return retValue;

	if (server){
	/* caller should check connection first. checkURL() in network.c file */

		result = curl_url_set(retValue, CURLUPART_URL, server, 0);
		if (result != CURLUE_OK){
			curl_url_cleanup(retValue);
			retValue = NULL;
			fprintf(stderr, "curl_url_set() failed: %s\n",
			            curl_url_strerror(result));
		}
	}

	return retValue;
}

/* initialQuery(): gets curl easy handle using curl URL parse handle.
 * Sets basic (common) query options.
 ***************************************************************** */
CURL * initialQuery (CURLU *serverUrl){  // curl parser url CURLU

	CURL			*qryHandle = NULL;
	CURLcode 	res;

	if (sessionFlag == 0){
		fprintf(stderr, "initialQuery(): Error, session not initialized. You must call\n "
				     " initialSession() first and check its return value.\n");
		return qryHandle;
	}

	ASSERTARGS (serverUrl);

	qryHandle = easyInitial();
	if ( ! qryHandle){
		fprintf(stderr, "initialQuery(): curl_easy_init() call failed. Client:: Abort?!\n");
		return qryHandle;
	}

	res = queryBasicOptions (qryHandle, serverUrl);
	if(res != CURLE_OK) {
		fprintf(stderr, "initialQuery(): Error returned from queryBasicOptions().\n");
		easyCleanup(qryHandle);
		qryHandle = NULL;
		return qryHandle;
	}

	return qryHandle;

}

/* queryBasicOptions(): sets query basic options
************************************************************************** */
CURLcode queryBasicOptions (CURL *qH, CURLU *serverUrl){

	CURLcode res;

	ASSERTARGS (qH && serverUrl);

	res = curl_easy_setopt(qH, CURLOPT_CURLU, serverUrl);
	if(res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set URL to srvrPath "
				  "{CURLOPT_CURLU}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt(qH, CURLOPT_USERAGENT, "curl/7.80.0");
	if(res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set USERAGENT "
	   				"{CURLOPT_USERAGENT}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt (qH, CURLOPT_TCP_KEEPALIVE, 1L);
	if(res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set KEEPALIVE connection"
				  "{CURLOPT_TCP_KEEPALIVE}: %s\n", curl_easy_strerror(res));
		return res;
	}

	/* turn on TRANSFER_ENCODING */
	res = curl_easy_setopt (qH, CURLOPT_TRANSFER_ENCODING, 1L);
	if(res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set TRANSFER_ENCODING"
	   				  "{CURLOPT_TRANSFER_ENCODING}: %s\n", curl_easy_strerror(res));
		return res;
	}

	// tell it to use POST http method -- third parameter set to one
	res = curl_easy_setopt(qH, CURLOPT_POST, 1L);
	if (res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set POST method "
				"{CURLOPT_POST}: %s\n", curl_easy_strerror(res));
		return res;
	}

	res = curl_easy_setopt(qH, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	if(res != CURLE_OK) {
		fprintf(stderr, "queryBasicOptions() failed to set WRITEFUNCTION "
	   				  "{CURLOPT_WRITEFUNCTION}: %s\n", curl_easy_strerror(res));
		return res;
	}

	return ztSuccess;

}

/* performQuery(): executes query on the srvrURL, writes results in memory
 * defined in answer pointer.
 *****************************************************************************/

int performQuery (MEMORY_STRUCT *answer, char *query, CURLU *srvrURL, CURL *qh){

	CURLcode	result;

	ASSERTARGS (answer && query && srvrURL && qh);

	answer->memory = malloc(1); /* should check return, lazy ass! */
	answer->size = 0;

	result = curl_easy_setopt(qh, CURLOPT_WRITEDATA, (void *)answer);
	if(result != CURLE_OK) {
		fprintf(stderr, "performQuery() failed to set WRITEDATA "
	   				  "{CURLOPT_WRITEDATA}: %s\n", curl_easy_strerror(result));
		return result;
	}

	// what to POST -- third parameter is the pointer to our query string
	result = curl_easy_setopt(qh, CURLOPT_POSTFIELDS, query);
	if (result != CURLE_OK) {
		fprintf(stderr, "performQuery() failed to set POSTFIELD "
				"{CURLOPT_POSTFIELDS}: %s\n", curl_easy_strerror(result));

		return result;
	}

	/* get it! */
	result = curl_easy_perform(qh);

	/* check for errors */
	if (result != CURLE_OK) {
		fprintf(stderr, "performQuery() failed call to curl_easy_perform!!: %s\n",
				curl_easy_strerror(result));
	}

	else {

		printf("performQuery(): Done.  "
				"%u bytes retrieved\n\n", (unsigned) answer->size);

	}

	return ztSuccess;

}
