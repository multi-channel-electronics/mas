/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _VERSION_H_
#define _VERSION_H_

/* This defines REPO_VERSION and BRANCH_ID */
#include "../../defaults/config.h"

/* The BRANCH_ID variable is intended to compensate for any changes in   */
/* repository (or whatever) that might cause version numbers to be       */
/* repeated.                                                             */


/* For major interface or behaviour changes, adjust the VERSION_#        */

#define VERSION_MAJOR           1
#define VERSION_MINOR           0

/* preprocessor magic to get strings from ints */
#define STRINGIFY(X) STRINGIFY1(X)
#define STRINGIFY1(X) #X

#define VERSION_STRING          STRINGIFY(VERSION_MAJOR) "." \
                                STRINGIFY(VERSION_MINOR) \
                                " [" BRANCH_ID ":" REPO_VERSION "]"

#endif
