/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#ifndef _VERSION_H_
#define _VERSION_H_

/* This defined BRANCH_ID and REPO_VERSION */
#include "../defaults/config.h"

/* The BRANCH_ID variable is intended to compensate for any changes in   */
/* repository (or whatever) that might cause version numbers to be       */
/* repeated.                                                             */

#define VERSION_STRING          BRANCH_ID ":" REPO_VERSION

#endif
