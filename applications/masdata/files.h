#ifndef _FILES_H_
#define _FILES_H_

#include "main.h"

/* File-modes: model for changing files during data taking.
 *
 * FMODE_SINGLE: no file changing:  file_today.dat
 * FMODE_SERIES: simple increment:  file_today_0.dat, etc.
 * FMODE_FRAME:  frame numbers:     file_today_000001.dat file_today_010001.dat
 * FMODE_CTIME:  posix time stamps: file_today_130231239.dat file_today_139234839
 *
 */

// enum {STAMP_INDEX , STAMP_FRAME, STAMP_CTIME};
enum {FMODE_SINGLE, FMODE_SERIES, FMODE_FRAME, FMODE_CTIME};

#define N_FILEMODES 4

extern struct string_pair filemode_pairs[N_FILEMODES];

typedef struct {

	int  mode;              /* FMODE_xxx */
	char basename[MEDLEN];  /* format string */
	char filename[MEDLEN];  /* active filename */

	int overwrite;          /* open as "w" instead of "a" */

	int index;              /* file count */
	int frames_max;         /* desired period of each file */
	int frames_next;        /* target frame index for next reopen */

	FILE *out;              /* output file stream */

} datafile_t;


int  data_closefile(datafile_t *f, char *errstr);
int  data_openfile(datafile_t *f, char *errstr);
int  data_setfilename(datafile_t *f, char *errstr, char *filename);
int  data_flush(datafile_t *f);

int data_set_directory(params_t *p, char *errstr, char *dirname);
int data_get_directory(params_t *p, char *dirname);

int data_set_basename(datafile_t *f, char *basename, char *errstr);
int data_set_filemode(datafile_t *f, char *mode_str, char *errstr);
int data_set_frameint(datafile_t *f, int frameint, char *errstr);


/* Data file mechanism */

int data_refile_start(datafile_t *f, int seq_first);
int data_refile_stop(datafile_t *f);
int data_refile_criterion(datafile_t *f, int frame);
int data_refile_now(datafile_t *f, int frame);

int data_refile_increment(datafile_t *f, int frame);
int data_refile_make_filename(datafile_t *f, int frame);
int data_refile_reopen(datafile_t *f);

#endif
