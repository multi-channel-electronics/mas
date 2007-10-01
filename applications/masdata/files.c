// _GNU_SOURCE needed for fopen64
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <libmaslog.h>

#include "data.h"
#include "files.h"
#include "masdata.h"

struct string_pair filemode_pairs[N_FILEMODES] = {
	{FMODE_SINGLE, "single"},
	{FMODE_SERIES, "series"},
	{FMODE_FRAME,  "frame"},
	{FMODE_CTIME,  "ctime"}
};

#define CHECKPTR        if (p==NULL) return -1

/*
 *  Output file handling
 *
 *  _openfile, _closefile, _setfilename are obsolete. Delete them.
 */

int data_openfile(datafile_t *f, char *errstr)
{
	if (f==NULL) return -1;

	if (f->out!=NULL) {
		strcpy(errstr, "File pointer in use (close first).");
		return -1;
	}

	f->out = (FILE*)fopen64(f->filename, "a");

	if (f->out==NULL) {
		sprintf(errstr, "Could not open file '%s'.",
			f->filename);
		return -1;
	}

	return 0;
}

int data_closefile(datafile_t *f, char *errstr)
{
	if (f==NULL) return -1;

	if (f->out==NULL) {
		strcpy(errstr, "File not open.");
		return -1;
	}
	
	FILE *temp = f->out;
	f->out = NULL;
	if (fclose(temp)!=0) {
		sprintf(errstr, "File close failed [%i]", errno);
		return -1;
	}

	return 0;
}

int data_setfilename(datafile_t *f, char *errstr, char *filename)
{
	if (f==NULL || filename==NULL) return -1;
	strcpy(f->filename, filename);
	return 0;
}


/* Obsolescence ends */

int data_flush(datafile_t *f)
{
	if (f==NULL || f->out==NULL) return -1;
	fflush(f->out);
	return 0;
}

int data_opendir(params_t *p, char *errstr)
{
	if (p->server.work_dir[0]==0) {
		strcpy(errstr, "No working directory specified.");
		return -1;
	}

	if (chdir(p->server.work_dir)!=0) {
		sprintf(errstr, "Could not change to '%s'.",
			p->server.work_dir);
		return -1;
	}

	data_get_directory(p, errstr);
	logger_print(&p->logger, errstr);
	
	return 0;
}

int data_set_directory(params_t *p, char *errstr, char *dirname)
{
	if (p==NULL) return -1;
	if (dirname!=NULL)
		strcpy(p->server.work_dir, dirname);

	return data_opendir(p, errstr);
}

int data_get_directory(params_t *p, char *msg)
{
	strcpy(msg, "Working directory is ");
	if (getcwd(msg + strlen(msg), MEDLEN-strlen(msg)-1)==NULL) {
		strcat(msg, "too long to fit in this string.");
	}
	return 0;
}

int data_set_filemode(datafile_t *f, char *mode_str, char *errstr)
{
	int key = -1;
	if (f!=NULL && mode_str!=NULL)
		key = lookup_key(filemode_pairs, N_FILEMODES, mode_str);
	
	if (key < 0) {
		sprintf(errstr,
			"file mode requires parameter from [ %s]",
			key_list_marker(filemode_pairs, N_FILEMODES, " ",
					f->mode, "*"));
		return -1;
	}

	f->mode = key;
	return 0;
}

int data_set_basename(datafile_t *f, char *basename, char *errstr)
{
	if (f==NULL) {
		sprintf(errstr, "Null datafile_t pointer!");
		return -1;
	}

	//Basic sanity check on filename
	if (basename==NULL || *basename==0) {
		sprintf(errstr, "'set basename' expects an argument");
		return -1;
	}

	//Check formats count
	char *s = basename;
	int fmts = 0;
	int fmts_expected = (f->mode == FMODE_SINGLE) ? 0 : 1;

	while (*s != 0)
		if (*s++ == '%') fmts++;
	if ( fmts != fmts_expected ) {
		sprintf(errstr, "'set basename' format string must have %i "
			"arguments", fmts_expected);
		return -1;
	}

	//Maybe it will work...
	strcpy(f->basename, basename);
	f->frames_next = 0;
	
	return 0;
}

int data_set_frameint(datafile_t *f, int frameint, char *errstr)
{
	if (f==NULL) {
		sprintf(errstr, "Null datafile_t* !");
		return -1;
	}

	if (frameint <= 0) {
		sprintf(errstr,
			"'set frameint' expects integer (currently %i)",
			f->frames_max);
		return -1;
	}

	f->frames_max = frameint;
	return 0;
}



/* data_refile_*
 *
 * These functions are called from the data thread to initialize
 * output files and switch files on the fly.
 *
 * The data thread calls _start with the first frame index, and then
 * periodically (every frame?) checks for _criterion != 0.  This
 * indicates that the file needs changing and data thread should then
 * call _increment.  After last frame, call _stop to close file.
 */

int data_refile_reopen(datafile_t *f)
{
	if (f==NULL) return -1;
	
	FILE *ftmp = f->out;
	f->out = NULL;
	if (ftmp != NULL) fclose(ftmp);

	if (f->overwrite)
		f->out = fopen64(f->filename, "w");
	else
		f->out = fopen64(f->filename, "a");

	if (f->out==NULL)
		return -2;

	return 0;
}

int data_refile_make_filename(datafile_t *f, int frame)
{
	switch (f->mode) {
	case FMODE_SINGLE:
		strcpy(f->filename, f->basename);
		break;

	case FMODE_SERIES:
		sprintf(f->filename, f->basename, f->index);
		break;
		
	case FMODE_FRAME:
		sprintf(f->filename, f->basename, frame);
		break;

	case FMODE_CTIME:
		sprintf(f->filename,  f->basename, time(NULL));
		break;

	default:
		return -1;
	}

	return 0;
}

int data_refile_increment(datafile_t *f, int frame)
{
	switch (f->mode) {
	case FMODE_SERIES:
	case FMODE_FRAME:

		f->frames_next = ( (frame / f->frames_max) + 1) *
			f->frames_max;
		f->index = (frame / f->frames_max);

	}
	return 0;
}


/* Returns non-zero if data_refile_now should be called */

int data_refile_criterion(datafile_t *f, int frame)
{
	if (f->mode == FMODE_SINGLE) return 0;

	return (frame >= f->frames_next);
}

int data_refile_now(datafile_t *f, int frame)
{
	if (f->mode == FMODE_SINGLE) return 0;

	data_refile_increment(f, frame);

	if (data_refile_make_filename(f, frame)!=0) {
		fprintf(stderr, "data_refile_make_filename failed!\n");
		return -1;
	}

	if (data_refile_reopen(f)!=0) {
		fprintf(stderr, "data_refile_reopen failed\n");
		return -2;
	}

	return 0;
}

int data_refile_start(datafile_t *f, int seq_first)
{
	if (f==NULL) return -1;

	// Sanity checks on file mode and switch interval
	switch (f->mode) {

	case FMODE_SINGLE:
	case FMODE_CTIME:
		break;

	case FMODE_SERIES:
	case FMODE_FRAME:
		if (f->frames_max < 1) return -1;
		break;

	default:
		return -1;
	}

	data_refile_increment(f, seq_first);

	int err = data_refile_make_filename(f, seq_first);
	if (err != 0) return err;

	return data_refile_reopen(f);
}

int data_refile_stop(datafile_t *f)
{
	if (f==NULL || f->out==NULL) return -1;

	FILE *ftmp = f->out;
	f->out = NULL;
	fclose(ftmp);

	return 0;
}
