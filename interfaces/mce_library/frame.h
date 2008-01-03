#ifndef __FRAME_H__
#define __FRAME_H__

#include <mce_library.h>

#define EXIT_LAST      1
#define EXIT_READ      2
#define EXIT_WRITE     3
#define EXIT_STOP      4
#define EXIT_EOF       5
#define EXIT_COUNT     6 

int sort_columns( mce_acq_t *acq, u32 *data );

#endif
