#ifndef _ACQ_H_
#define _ACQ_H_

#include "frame.h"

/* MCE data acquisition modes. */

#define MCEDATA_FILESTREAM        (1 <<  0) /* output to file */
#define MCEDATA_INCREMENT         (1 <<  1) /* keep ret_data_s up to date */
#define MCEDATA_FILESEQUENCE      (1 <<  2) /* switch output files regularly */
#define MCEDATA_THREAD            (1 <<  3) /* use non-blocking data thread */


/* MCE card bits - fix this! */

#define MCEDATA_RC1               (1 <<  0)
#define MCEDATA_RC2               (1 <<  1)
#define MCEDATA_RC3               (1 <<  2)
#define MCEDATA_RC4               (1 <<  3)
#define MCEDATA_RCS               (MCEDATA_RC1 | MCEDATA_RC2 | MCEDATA_RC3 | MCEDATA_RC4)

#define MCEDATA_CARDS             4
#define MCEDATA_COMBOS            (1 << MCEDATA_CARDS)

#define MCEDATA_COLUMNS           8
#define MCEDATA_ROWS              41

#define MCEDATA_HEADER            43
#define MCEDATA_FOOTER            1

/* Card bits to use for the rcs_to_report_data register */
#define MCEDATA_RCSFLAG_RC1       (1 << 5)
#define MCEDATA_RCSFLAG_RC2       (1 << 4)
#define MCEDATA_RCSFLAG_RC3       (1 << 3)
#define MCEDATA_RCSFLAG_RC4       (1 << 2)

struct mce_acq_struct;
typedef struct mce_acq_struct mce_acq_t;

struct mcedata_storage;
typedef struct mcedata_storage mcedata_storage_t;


struct mcedata_storage {

	int (*init)(mce_acq_t*);
	int (*pre_frame)(mce_acq_t *);
	int (*flush)(mce_acq_t *);
	int (*post_frame)(mce_acq_t *, int, u32 *);
	int (*cleanup)(mce_acq_t *);
	int (*destroy)(mcedata_storage_t *);

	void* action_data;

};

struct mce_acq_struct {

	mce_context_t *context;

	int n_frames;
	int n_frames_complete;
	int frame_size;                 // Active frame size

	int status;
	int cards;                      // Bit mask of RCs reporting
	int n_cards;                    // Number of RCs reporting
	int rows;                       // Number of rows reported
	int cols;                       // Number of columns reported
	int row0[MCEDATA_CARDS];        // Index, by RC, of first row reported
	int col0[MCEDATA_CARDS];        // Index, by RC, of first column reported
	int data_mode[MCEDATA_CARDS];   // Data modes, by card.

	mcedata_storage_t* storage;

	char errstr[MCE_LONG];

	int timeout_ms;
 	int options;

	mce_param_t ret_dat;
	mce_param_t ret_dat_s;

	int last_n_frames;

	frame_header_abstraction_t *header_description;

	int ready;
};

#endif
