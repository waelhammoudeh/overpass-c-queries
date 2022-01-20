/*
 * util.c
 *
 *  Created on: Jun 23, 2017
 *      Author: wael
 *  small functions ... system calls ... strings functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>   /* gettimeofday() */
#include <ctype.h>	//toupper()
#include <sys/wait.h>

#include "util.h"
#include "ztError.h"
#include "dList.h"

/* function source was: WRITING SOLID CODE by Steve Maguire */
void AssertArgs (const char *func, char *file, int line){

	fflush(NULL);
	fprintf (stderr, "\nArgument(s) Assertion failed [NULL]! "
			"In Function: %s()\n", func);
	fprintf (stderr, "   File: < %s > , At Line: %d\n", file, line);
	fflush(stderr);
	abort();

}  /* END AssertArgs()  */

/* IsEntryDir(): function to test if the specified entry is a directory or not.
 * Argument: entry : entry to test for.
 * Return: TRUE if entry is a directory, FALSE otherwise.
 *
 */

int IsEntryDir (char const *entry) {

	struct stat status;

	if (entry == NULL)
		return FALSE;

	errno = 0;

	if (lstat (entry, &status) !=  0){
		/* fill status structure, lstat returns zero on success */
		fprintf (stderr, "IsEntryDir(): Could NOT lstat entry:  %s . "
				"System says: %s\n",
				entry, strerror(errno));
		return FALSE;
	}

	 if (S_ISDIR (status.st_mode))

		 return TRUE;

	 return FALSE;

}  /* END IsEntryDir()  */

/* file name can not start with a digit or hyphen,
 * no funny characters including space and tab.
   path and file name should be less than 255 characters long.
   Returns FALSE for bad name and TRUE for good name.
   NEED TO ADD ERRRRRRRRRORRRRRRR COOOODES @@@@@@@@@@@@@@@@@@@@@@@
   TO DO: we should check each part of path at a time!!!
   returns TRUE or FALSE
******************************************************************/

int IsGoodFileName (char *name){

	char *digits = "0123456789";
	char *disallowed = "<>|,:()&;?*!@#$%^+=\040\t";
                   /* bad characters in file name, space and tab included */
	char hyphen = '-';

	char tmpBuffer[PATH_MAX]= {0};
	char *part;

	if (name == NULL)

		return FALSE;

	if (name[0] == hyphen) //file name can not start with hyphen

		return FALSE;

	if ( strlen(name) > PATH_MAX )  /* path + name */

		return FALSE;

	strcpy (tmpBuffer, name);

	part = strtok (tmpBuffer, "/");

	while (part){
		/* printf ("IsGoodFileName(): part is %s\n", part); */

		if ( strpbrk(part, digits) == part ) // do not start with digit

			return FALSE;

		if ( strpbrk(part, disallowed) )

			return FALSE;

		part = strtok (NULL, "/");
	}

	return TRUE;

}   /* END IsGoodFileName() */

int IsGoodDirectoryName(char *name){

	if (name == NULL)

		return FALSE;

	if (IsSlashEnding(name))

		name[strlen(name) - 1] = '\0';

	return IsGoodFileName(name);

}

/* LastOfPath() : function to extract last entry of its path argument.
 * path should be less than or equal to PATH_MAX, can end with a slash,
 * in that case it is removed and entry before it is returned.
 * This function allocates memory for buffer and returns its pointer.
 *  @@@@@
 * Modified on 9/10/15
 * Still need work to streamline and simplify. This function is called
 * by many many other functions. It should work like BASH basename
 * function.
 * ***************************************************************** */

char* lastOfPath (char *path){

	char  *lastSlash;
	char  *ret;
	char  buf[PATH_MAX + 1] = {0};
	/* char  first; */
	char  bkSlash = '/';

	ASSERTARGS(path);

	if (strlen(path) > (PATH_MAX))

		return NULL;

	if ((strlen(path) == 1) && (path[0] == '/')) { /* if only back slash */
		printf ("LastOfPath(): Error: strlen(path) is one AND it is (/).\n");
		return NULL;
	}

	if ( ! IsGoodFileName(path)){
		printf ("LastOfPath(): Error: argument path or part of it is NOT good file name.\n");
		return NULL;
	}

	/* if path has NO back slash, just return a copy of path */
	if (strchr(path, bkSlash) == NULL){

		ret = (char*) malloc (strlen(path) + 1);
		if (ret == NULL){
			printf ("LastOfPath(): Error allocating memory.\n");
			return NULL;
		}
		strcpy (ret, path);
		//printf ("LastOfPath(): no bkSlash in path, returning copy ret: %s\n", ret);
		return ret;
	}

	/**
	first = path[0];

	if (first != '/'){
		printf ("LastOfPath(): Error: Not a path; first is NOT a slash.\n");
		return NULL;
	}
	**/

	/* copy path to buf, if path ends with back slash then do NOT include it in copy */
	if ( *(path + strlen(path) - 1) == '/')
		strncpy (buf, path, strlen(path) - 1);
	else
		strcpy (buf, path);

	lastSlash = strrchr (buf, '/');

	if (lastSlash == NULL)
		return NULL;

	lastSlash++;

	ret = (char*) malloc (strlen(lastSlash) + 1);
	if (ret == NULL){
		printf ("LastOfPath(): Error allocating memory.\n");
		return NULL;
	}

	strcpy (ret, lastSlash);

	return ret;

}  /*  END LastOfPath()  */

/* DropExtension(): drops extension from str in; anything after a right period.
 * allocates memory for return pointer. returns NULL if str > PATH_MAX or
 * memory allocation error. you get a duplicate if str has no period.
 **************************************************************************/
char* DropExtension(char *str){

	char		*retCh = NULL;
	int	 	period = '.';
	char		*periodCh;
	char 	temp[PATH_MAX] = {0};

	ASSERTARGS(str);

	if(strlen(str) > PATH_MAX)

		return retCh;

	periodCh = strrchr(str, period);

	if(periodCh)

		strncpy(temp, str, strlen(str) - strlen(periodCh));

	else

		strcpy(temp, str);

	retCh = (char *)malloc(sizeof(char) * (strlen(temp) + 1));
	if(! retCh)

		return retCh;

	strcpy(retCh, temp);

	return retCh;

}

/* allocate2x2(): allocates memory for 2 dimensional array,
 * array is with dimension row x col, and element size is elemSize
 * allocates one extra element and sets it to NULL.
 * all elements are initialed empty.
 * row, col and elemSize must be > 0 */

void** allocate2Dim (int row, int col, size_t elemSize){

	void		**array = NULL;
	void 	**mover;
	int 		iRow, jCol;

	if(row < 1 || col < 1 || elemSize < 1){
		printf("allocate2Dim(): ERROR at least one parameter is less than 1\n"
				"returning NULL\n");
		return array;
	}

	array = (void**) malloc(sizeof(void*) * (row * col) + 1);
	if(array == NULL)
		return array;

	mover = array;
	for (iRow = 0; iRow < row; iRow++){
		for (jCol = 0; jCol < col; jCol++){
			*mover = (void*) malloc (elemSize);
			if (*mover == NULL){
				array = NULL;
				return array;
			}
			memset(*mover, 0, elemSize);
			mover++;
		}
	}
	*mover = NULL;  /* set last one to NULL. */

//printf("allocate2x2(): returning array :: bottom of function\n\n");

	return array;
}

/* free2Dim(): call only to free allocate2Dim() */
void free2Dim (void **array, size_t elemSize){

	void **mover;

	ASSERTARGS (array); // abort if not array!!

	mover = array;

	while(*mover){

		memset (*mover, 0, elemSize);
		free(*mover);
		mover++;
	}

	free(array);

} // END free2Dim()

/* removeSpaces(): removes leading and trailing space (space and tabs)
 * from str. returns 1 on success - almost always. returns 0 (zero) and
 * does nothing to str if it is all white space.
 *********************************************************************/
int removeSpaces(char **str){

	char		SPACE = '\040';
	char		TAB = '\t';
	char		*SPACESET = "\040\t";
	char		*tmp;

	ASSERTARGS(str);

	tmp = *str;

	// return 0 if all spaces
	if(strspn(tmp, SPACESET) == strlen(tmp))

		return 0;

	if (strlen(tmp) == 1) // passed above test, single character is not space

		return 1;

	while(tmp[0] == SPACE || tmp[0] == TAB)

		tmp++;

	while(tmp[strlen(tmp) - 1] == SPACE ||
		  tmp[strlen(tmp) - 1] == TAB)

		tmp[strlen(tmp) - 1] = '\0';

	*str = tmp;

	return 1;

} // END removeSpaces()

/* from Advanced-Linux Programming --- still needs work */
char* get_self_executable_directory (){

	int 		rval;
	char 	link_target[1024];
	char		*last_slash;
	size_t 	result_length;
	char		*result;

	/* Read the target of the symbolic link /proc/self/exe. */
	rval = readlink ("/proc/self/exe", link_target, sizeof (link_target));
	if (rval == -1)
		/* The call to readlink failed, so bail. */
		return NULL; //abort ();
	else
		/* NUL-terminate the target. */
		link_target[rval] = '\0';

	/* We want to trim the name of the executable file, to obtain the
	   directory that contains it. Find the rightmost slash. */
	last_slash = strrchr (link_target, '/');
	if (last_slash == NULL || last_slash == link_target)
		/* Something strange is going on. */
		abort ();

	/* Allocate a buffer to hold the resulting path. */
	result_length = last_slash - link_target;
	result = (char*) malloc (result_length + 1);
	if(result == NULL){

		return NULL;
	}

	/* Copy the result. */
	strncpy (result, link_target, result_length);
	result[result_length] = '\0';

	return result;

}
/* IsArgUsableFile(): is argument a usable input file?
 *   tests its argument pointed to by name for the following:
 * 1) is it a good file name
 * 2) does it exist
 * 3) is it readable
 * 4) is it a regular file
 * function returns ztSuccess if all tests are ok, or one of the
 * following error codes:
 *   ztBadFileName
 *   ztNotFound
 *   ztNoReadPerm
 *   ztNotRegFile
 *   ztNullArg
 *
 * function is usually used to test input file argument.
 */
int IsArgUsableFile(char* const name){

	struct stat fileInfo;

	if (name == NULL)

		return ztNullArg;

	if (!IsGoodFileName(name))

		return ztBadFileName;

	// does it exist
	if(access(name, F_OK) != 0)

		return ztNotFound;

	/* can we read it? */
	if (access(name, R_OK) != 0)

		return ztNoReadPerm;

	/* is it a regular file */
	if (stat (name, &fileInfo) != 0 || !S_ISREG (fileInfo.st_mode))

		return ztNotRegFile;

	if (fileInfo.st_size == 0)

		return ztFileEmpty;

	return ztSuccess;

} // END IsArgUsableFile()

int IsArgUsableDirectory(char* const name){

	struct stat dirInfo;

	// does it exist
	if(access(name, F_OK) != 0)

		return ztNotFound;

	/* can we access it? */
	if (access(name, R_OK | W_OK | X_OK) != 0)

		return ztInaccessibleDir;

	/* is it a directory? */
	if (stat (name, &dirInfo) != 0 || !S_ISDIR (dirInfo.st_mode))

		return ztPathNotDir;

	return ztSuccess;

} // END IsArgUsableDirectory()

/* MyMkDir(): function to create the argument specified directory using the
 * permission defined (in util.h), if the directory exist this function will
 * return WITHOUT error. It is NOT an error if it exist. Function will NOT
 * make parent.
 */
int myMkDir (char *name){

	int result;

	errno = 0;
	result = mkdir (name, PERMISSION);

	if ( (result ==  -1) && (errno == EEXIST) )

		return ztSuccess;

	else if (result == -1){

		printf ("MyMkDir(): Error mkdir %s, system says: %s\n",
				name, strerror(errno));

		return ztFailedSysCall;
	}

	return ztSuccess;

}   /* END MyMkDir()  */

/* myFgets(): works like fgets() except it skips comments and blank lines
 * and increments lineNum.
 * comment line: first non-white character is <; or #>
 * TODO document function!
 ************************************************************************/
char* myFgets(char *strDest, int count, FILE *fPtr, int *lineNum){

	char		*whiteSpace = "\040\t\n\r";  /* white space is made of:
																space, tab, line feed
																and carriage return. */
	char 	SPACE = '\040';
	char 	TAB   = '\t';
	//char *rvalue;
	char 	*str;

	str = (char *) malloc(sizeof(char) * LONG_LINE);
	memset (str, 0, LONG_LINE);


	/* read one line at a time, if we fail, then file ends prematurely */
	while(fgets(str, count, fPtr)) {

		(*lineNum)++; // increment line count

		// ignore blank lines
		if (strspn(str, whiteSpace) == strlen(str))

			continue;

		/* move str/line pointer to first non-white character */
		while(str[0] == SPACE || str[0] == TAB)

			str++;

		/* ignore comments lines, comment char is ';' *
		* if ((strchr(str, COMMENT_CHAR) == str))
		* added COMMENT_SET on 12/18/2018 to include # plus ;
		* COMMENT_SET is defined in util.h file w.h */

		if ((strpbrk(str, COMMENT_SET) == str))

			continue;

		if (strlen(str)) // has something still - other than blank or comments

			break; // we can return here - it is much clearer in the bottom
	}

	/* IMPORTANT ****:
	 * fgets() returns NULL on EOF and on File Error, do the same */
	if(feof(fPtr) || ferror(fPtr))

		return NULL;

/* operating system sets last line to \n
	if((strlen(str) == 1) && (str[0] == '\n')){
		printf("myFgets(): strlen(str) IS One AND is line feed ...\n");
		return NULL;
	}
*****************************************************************/

	strcpy (strDest, str);

	return strDest;

} // END myFgets()

/* is string good double string? all digits, plus, minus and period */
int isStrGoodDouble(char *str){

	char *allowed = "-+.0123456789";

	if (strspn(str, allowed) != strlen(str))

		return FALSE;

	return TRUE;
}

/* getHome(): function gets home directory for effective user.
 *
 * The simple method is to pull the environment variable "HOME"
 * The slightly more complex method is to read it from the system user database.
 *
 */

char* getHome(){

	char		*resultDir = NULL;

	char		*homeDir = getenv("HOME");

	if (homeDir != NULL && IsArgUsableDirectory(homeDir)){

		resultDir = strdup(homeDir);
		return resultDir;
	}

	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);

	ASSERTARGS(pw && pw->pw_dir);

	homeDir = pw->pw_dir;

	if (IsEntryDir(homeDir))
		resultDir = strdup(homeDir);

	return resultDir;

}

/* getUserName(): returns effective user name, abort()s if unsuccessful */

char *getUserName(){

	char *user = NULL;

	user = getenv("USER");

	if (user)

		return user;

	uid_t uid = getuid();
	struct passwd *pw = getpwuid(uid);

	ASSERTARGS(pw && pw->pw_dir);

	user = pw->pw_name;

	ASSERTARGS(user);

	return user;
}

int mySpawn (char *prgName, char **argLst){

	pid_t		childPID;

	ASSERTARGS(prgName && argLst);

	childPID = fork();

	if (childPID == 0){   /* this is the child process */
	/*	prototype from man page:
	 * int execv(const char *path, char *const argv[]); */

		execv (prgName, argLst);

		/* The execv function returns only if an error occurs. */
		printf ("mySpawn(): I am the CHILD!!!\n"
				    "If you see this then there was an error in execv\n");
		fprintf (stderr, "mySpawn(): an error occurred in execv\n");
		abort();
	}
	else

		return childPID;
}

int spawnWait (char *prog, char **argsList){

	//pid_t		childPid;
	int		childStatus;

	// don't allow nulls
	ASSERTARGS(prog && argsList);

	// ignore child PID returned by mySpawn()
	mySpawn(prog, argsList);

	// wait for the child to finish
	wait(&childStatus);

	//if (WIFEXITED(childStatus)) // normal exit
	if (WEXITSTATUS(childStatus) == EXIT_SUCCESS)

		return ztSuccess;

	else {

		printf("spawnWait(): Error child process exited abnormally! With exit code: %d\n",
				   WEXITSTATUS(childStatus));
		// TODO: maybe get system error; careful what you wish for, is that a system error?
		return ztChildProcessFailed;
	}
}

/* myGetDirDL() function to read directory and place entries in sorted list.
 * The returned list WILL INCLUDE the full entry path.
 * This function will NOT include the dot OR double dot entries.
 * caller allocates and initials dstDL.
 * checked run time error for dstDL to be empty and for dir to point to a
 * directory and be accessible.
 * **************************************************************************/

int myGetDirDL (DL_LIST *dstDL, char *dir){

	DIR 			*dirPtr;
	struct 		dirent 	*entry;
	char 		dirPath[PATH_MAX + 1];
	char			tempBuf[PATH_MAX + 1];
	char 		*fullPath;
	int	 		result;

	ASSERTARGS (dstDL && dir);

	if (DL_SIZE(dstDL) != 0){
		printf("myGetDirDL(): Error list not empty.\n");
		return ztListNotEmpty;
	}

	if ( ! IsEntryDir(dir)){
		printf ("myGetDirDL(): Error specified argument is not a directory.\n");
		return ztPathNotDir;
	}

	if (IsArgUsableDirectory(dir) != ztSuccess){
		printf ("myGetDirDL(): Error specified directory not usable.\n");
		return ztInaccessibleDir;
	}

	dirPtr = opendir (dir);
	if (dirPtr == NULL){  		/* tell user why it failed */
		printf ("myGetDirDL(): Error opening directory %s, system says: %s\n",
				dir, strerror(errno));
		return ztFailedSysCall;
	}

	/* append a slash if it is not there. */
	if ( dir[ strlen(dir) - 1 ] == '/' )
		sprintf (dirPath, "%s", dir);
	else
		sprintf (dirPath, "%s/", dir);

	while ( (entry = readdir (dirPtr)) ){

		if ( (strcmp(entry->d_name, ".") == 0) ||
			 (strcmp(entry->d_name, "..") == 0) )

			continue;

		ASSERTARGS (snprintf (tempBuf, PATH_MAX, "%s%s",
				    dirPath, entry->d_name) < PATH_MAX);

		fullPath = (char *)malloc(strlen(tempBuf) + 1);
		if( ! fullPath){
			printf("myGetDirDL(): error allocating memory.\n");
			return ztMemoryAllocate;
		}
		strcpy(fullPath, tempBuf);
		result = ListInsertInOrder (dstDL, fullPath);
		if (result != ztSuccess){
			printf("myGetDirDL(): Error returned by ListInsertInOrder().\n");
			printf(" Message: %s\n\n", code2Msg(result));
			return result;
		}
	}

	closedir(dirPtr);

	return ztSuccess;

}  /* END myGetDirDL()  */

void zapString(void **data){ //free allocated string as in myGetDirDL()

	char *str;

	ASSERTARGS (data);

	str = *data;
	if(str)

		free(str);

	return;
}
/* GetFormatTime(): formats current time in a string buffer, allocates buffer
 * and return buffer address.  */

char* getFormatTime (void){

	char		*ret = NULL;
	char		buffer[1024];
	char  	timeBuf[80];
	long    milliSeconds;
	struct    timeval  startTV;   /* timeval has two fields: tv_sec: seconds  and tv_usec: MICROseconds */
	struct    tm *timePtr;

	gettimeofday (&startTV, NULL);
	timePtr = localtime (&startTV.tv_sec);
	strftime (timeBuf, 80, "%a, %b %d, %Y %I:%M:%S %p", timePtr);
	milliSeconds = startTV.tv_usec / 1000;

	sprintf (buffer, "%s :milliseconds: %03ld", timeBuf, milliSeconds);

	ret = (char *) malloc ((strlen(buffer) + 1) * sizeof(char));
	if (ret == NULL){
		printf ("getFormatTime(): Error allocating memory.\n");
		return ret;
	}

	strcpy (ret, buffer);

	return ret;

} /* END GetFormatTime() */
/****************************** StringToLower() ******************************
stringToLower: Function to convert a string to lower character case.
Argument: dest is a pointer to pointer to character for destination string.
          str is a pointer to source string.
Return: ztSuccess on success. ztMemoryAllocate on memory failure or ztInvalidArg
when source string length is zero.
This function allocates memory for destination. Error is returned on memory
failure.
******************************************************************************/

int stringToLower (char **dest, char *str){

  char 	*mover;

  ASSERTARGS (dest && str);

  if (strlen(str) == 0){
	  printf("stringToLower(): Error empty str argument! Length of zero.\n");
	  return ztInvalidArg;
  }

  *dest = (char *) malloc (sizeof(char) * (strlen(str) + 1));
  if (dest == NULL){
	  printf("stringToLower(): Error allocating memory.\n");
	  return ztMemoryAllocate;
  }

  strcpy (*dest, str);

  mover = *dest;

  while ( *mover ){
    *mover = tolower(*mover);
    mover++;
  }

  return ztSuccess;

}  /* END StringToLower() */

int stringToUpper (char **dst, char *str){

	char 	*mover;

	ASSERTARGS (dst && str);

	if (strlen(str) == 0){
		printf("stringToUpper(): Error empty str argument! Length of zero.\n");
		return ztInvalidArg;
	}

	*dst = (char *) malloc (sizeof(char) * (strlen(str) + 1));
	if (dst == NULL){
		printf ("stringToUpper(): Error, allocating memory.\n");
		return ztMemoryAllocate;
	}

	strcpy (*dst, str);

	mover = *dst;

	while ( *mover ){
		if (*mover > 127){
			printf ("stringToUpper(): Error!! A character is larger that largest ASCII 127 decimal.\n");
			printf ("stringToUpper(): character is: <%c>, ASCII value: <%d>. String is: <%s>\n\n",
					*mover, *mover, str);
			return ztInvalidArg;
		}
		*mover = toupper(*mover);
		mover++;
	}

	return ztSuccess;

}  /* END stringToUpper() */

/* mkOutFile(): make output file name, sets dest to givenName if it has a slash,
 * else it appends givenName to rootDir and then sets dest to appended string
 */
int mkOutputFile (char **dest, char *givenName, char *rootDir){

	char		slash = '/';
	char		*hasSlash;
	char		tempBuf[PATH_MAX] = {0};

	ASSERTARGS (dest && givenName && rootDir);

	hasSlash = strchr (givenName, slash);

	if (hasSlash)

		*dest = (char *) strdup (givenName); // strdup() can fail .. check it FIXME

	else {

		if(IsSlashEnding(rootDir))

			snprintf (tempBuf, PATH_MAX -1 , "%s%s", rootDir, givenName);
		else
			snprintf (tempBuf, PATH_MAX - 1, "%s/%s", rootDir, givenName);

		*dest = (char *) strdup (&(tempBuf[0]));

	}

	/* mystrdup() ::: check return value of strdup() TODO */

	return ztSuccess;
}

/* openOutputFile(): opens filename for writing, filename includes path */

FILE* openOutputFile (char *filename){

	FILE		*fPtr = NULL;

	ASSERTARGS (filename);

	errno = 0; //set error number

	//try to open the file for writing
	fPtr = fopen(filename, "w");
	if (fPtr == NULL){

		fprintf (stderr, "openOutputFile(): Error opening file: <%s>\n", filename);
		fprintf(stderr, "System error message: %s\n\n", strerror(errno));
		//print reason with perror()
		perror("The call to fopen() failed!");
	}

	return fPtr;

} // END openOutputFile()
