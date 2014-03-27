/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */

/* multisync.c
 *
 * This is a storage module that provides generic support for writing
 * of data to multiple outputs (e.g. a flatfile and a rambuffer; a
 * flatfile and a dirfile.
 *
 * It basically just maintains a list of mce_acq_t objects, and calls
 * them in sequence for each storage action.
 *
 * Note that a superior interface would just pass around / store
 * mce_storage_t objects, each of which would have a pointer to the
 * parent acquisition structure.  That would be better, but would
 * require changing the whole API.  Some day.
 */


#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>

#include "context.h"


#define MAX_SYNCS  10


/* Maintain a list of storage objects.  Note that this list is a set
 * of pointers, whose non-null entries must start at index 0 and be
 * contiguous (i.e. there can't be holes in the list).
 */

typedef struct multisync_struct {
    int max_syncs;
    int stopped[MAX_SYNCS];
    int options[MAX_SYNCS];
    mce_acq_t *syncs[MAX_SYNCS];

    multisync_err_callback_t err_callback;
    void *user_data;
} multisync_t;


/* Methods */

/* Use a template for all the ones that just want an mce_acq_t*: */

#define DECLARE_MULTISYNC(MEMBER,STOP_OVERRIDE) \
  static int multisync_ ## MEMBER(mce_acq_t* acq)               \
  {                                                             \
    multisync_t *f = (multisync_t*)acq->storage->action_data;   \
    int i, err = 0;                                             \
                                                                \
    for (i=0; i<f->max_syncs && f->syncs[i] != NULL; i++) {     \
        if (f->syncs[i]->storage->MEMBER == NULL)               \
            continue;                                           \
        if (!(STOP_OVERRIDE) && f->stopped[i])                  \
            continue;                                           \
        err = f->syncs[i]->storage->MEMBER(f->syncs[i]);        \
        if (err != 0) {                                         \
            if (f->err_callback)                                \
                f->stopped[i] = f->err_callback(f->user_data,   \
                        i, err, mcedata_acq_ ## MEMBER);        \
            break;                                              \
        }                                                       \
    }                                                           \
    return err;                                                 \
}

DECLARE_MULTISYNC(init, 0)
DECLARE_MULTISYNC(cleanup, 1)
DECLARE_MULTISYNC(pre_frame, 0)
DECLARE_MULTISYNC(flush, 0)

static int multisync_post_frame(mce_acq_t *acq, int frame_index, uint32_t *data)
{
	multisync_t *f = (multisync_t*)acq->storage->action_data;
    int i, err = 0;

    for (i=0; i<f->max_syncs && f->syncs[i] != NULL; i++) {
        if (f->syncs[i]->storage->post_frame == NULL)
            continue;
        if (f->stopped[i])
            continue;
        err = f->syncs[i]->storage->post_frame(f->syncs[i], frame_index, data);
        if (err != 0) {
            if (f->err_callback)
                f->stopped[i] = f->err_callback(f->user_data, i, err,
                        mcedata_acq_post_frame);
            break;
        }
    }
    return err;
}


/* Generic destructor (not to be confused with cleanup member function) */

static int multisync_destructor(mcedata_storage_t *storage)
{
    int i, err;
	multisync_t *f = (multisync_t*)storage->action_data;
    if (f == NULL)
        return 0;

    // Perhaps this cascade of destruction should be optional.
    for (i=0; i<f->max_syncs && f->syncs[i] != NULL; i++) {
        if (f->syncs[i]->storage->destroy == NULL)
            continue;
        err = f->syncs[i]->storage->destroy(f->syncs[i]->storage);
        // Also free the acq structure (see _add function)
        free(f->syncs[i]);
        f->syncs[i] = NULL;
        if (err != 0)
            break;
    }

	// Free the private data for the storage module, clear the structure
    free(f);

	memset(storage, 0, sizeof(*storage));
	return 0;
}


/* Must provide every method of mcedata_storage! */

mcedata_storage_t multisync_actions = {
	.init = multisync_init,
    .pre_frame = multisync_pre_frame,
	.flush = multisync_flush,
	.post_frame = multisync_post_frame,
	.cleanup = multisync_cleanup,
	.destroy = multisync_destructor,
};


mcedata_storage_t* mcedata_multisync_create(int options)
{
	multisync_t *f = (multisync_t*)malloc(sizeof(multisync_t));
	mcedata_storage_t *storage =
		(mcedata_storage_t*)malloc(sizeof(mcedata_storage_t));
	if (f==NULL || storage==NULL) return NULL;

	//Initialize storage with the file operations, then set local data.
	memcpy(storage, &multisync_actions, sizeof(multisync_actions));
	storage->action_data = f;

    //Make sure all pointers are NULL, and n_sync = 0.
	memset(f, 0, sizeof(*f));
    f->max_syncs = MAX_SYNCS;
	return storage;
}

/* Allow user to add a sync. */

int mcedata_multisync_add(mce_acq_t *multisync_acq,
                          mcedata_storage_t *sync)
{
    int i, error = 0;
	multisync_t *f = (multisync_t*)multisync_acq->storage->action_data;
    if (f == NULL)
        return -1;

    // Append this sync to the list.
    for (i=0; i < f->max_syncs; i++) {
        if (f->syncs[i] == NULL) {
            // Duplicate the main acq, and replace the storage -- this is not ideal.
            // Don't forget to free it later, but just the main pointer.  Blech.
            mce_acq_t *acq = mcedata_acq_duplicate(multisync_acq);
            if (acq == NULL)
                return -1;
            acq->storage = sync;
            f->syncs[i] = acq;
            if (sync->init != NULL)
                error = sync->init(acq);
            return error;
        }
    }

    // No room left...
    return -2;
}

/* set a user-provided callback for when acq's fail */
void mcedata_multisync_errcallback(mce_acq_t *multisync_acq,
        multisync_err_callback_t callback, void *user_data)
{
    multisync_t *f = (multisync_t*)multisync_acq->storage->action_data;
    f->err_callback = callback;
    f->user_data = user_data;
}
