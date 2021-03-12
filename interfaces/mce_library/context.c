#include <stdlib.h>
#include <mce_library.h>

#include "version.h"


mce_context_t* mcelib_create()
{
	mce_context_t* c = (mce_context_t*)malloc(sizeof(mce_context_t));
	if (c == NULL) return c;

	logger_connect(&c->logger, NULL, "lib_mce");

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

	logger_close(&context->logger);

	free(context);
}


char* mcelib_version()
{
	return VERSION_STRING;
}
