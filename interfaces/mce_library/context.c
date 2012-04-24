/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <mce/defaults.h>

#include "context.h"
#include "version.h"
#include "../defaults/config.h"


mce_context_t* mcelib_create(int fibre_card)
{
  char *ptr;
	mce_context_t* c = (mce_context_t*)malloc(sizeof(mce_context_t));

	if (c == NULL) return c;

  if (fibre_card == MCE_DEFAULT_MCE)
    c->fibre_card = mcelib_default_fibre_card();
  else
    c->fibre_card = fibre_card;

#if MULTICARD
  ptr = mcelib_shell_expand("lib_mce[${MAS_CARD}]", c->fibre_card);
#else
  ptr = strdup("lib_mce");
#endif
	c->maslog = maslog_connect(NULL, ptr);
  free(ptr);

	c->cmd.connected = 0;
	c->data.connected = 0;
	c->config.connected = 0;

	return c;
}


void mcelib_destroy(mce_context_t* context)
{
	if (context==NULL) return;

	mceconfig_close(context);
	mcedata_close(context);
	mcecmd_close(context);

	maslog_close(context->maslog);

	free(context);
}


char* mcelib_version()
{
	return VERSION_STRING;
}
