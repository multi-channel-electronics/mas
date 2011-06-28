#ifndef _VERSION_H_
#define _VERSION_H_

/* This file is generated automatically from version.h.template          */

/* The REPO_NAME variable is intended to compensate for any changes in   */
/* repository (or whatever) that might cause version numbers to be       */
/* repeated.                                                             */


/* For major interface or behaviour changes, adjust the VERSION_#        */

#define VERSION_MAJOR           1
#define VERSION_MINOR           0

#define REPO_NAME               "MAS/trunk"
#define REPO_VERSION            "550M"


/* preprocessor magic to get strings from ints */
#define STRINGIFY(X) STRINGIFY1(X)
#define STRINGIFY1(X) #X

#define VERSION_STRING          STRINGIFY(VERSION_MAJOR) "." \
	                        STRINGIFY(VERSION_MINOR) \
	                        " [" REPO_NAME ":" REPO_VERSION "]"

#endif
