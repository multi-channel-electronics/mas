#include <stdlib.h>
#include <string.h>
#include <mce_library.h>
#include <mce/defaults.h>

#include "version.h"
#include "../defaults/config.h"


mce_context_t* mcelib_create(int fibre_card, const char *mas_config)
{
  char *ptr;
	mce_context_t* c = (mce_context_t*)malloc(sizeof(mce_context_t));
  c->mas_cfg = (struct config_t *)malloc(sizeof(c->mas_cfg));
  config_init(c->mas_cfg);

	if (c == NULL) return c;

  if (fibre_card == MCE_DEFAULT_CARD)
    c->fibre_card = mcelib_default_fibre_card();
  else
    c->fibre_card = fibre_card;

  /* load mas.cfg */
  if (mas_config != NULL) {
    if (!config_read_file(c->mas_cfg, mas_config)) {
      fprintf(stderr, "mcelib: Could not read config file '%s'\n", mas_config);
      return NULL;
    }
  } else {
    char *ptr = mcelib_default_masfile();
    if (ptr == NULL)
      fprintf(stderr, "Unable to obtain path to default configfile!\n");
    else if (!config_read_file(c->mas_cfg, ptr)) {
      fprintf(stderr, "mcelib: Could not read default configfile '%s'\n", ptr);
      return NULL;
    }
    free(ptr);
  }

#if MULTICARD
  ptr = mcelib_shell_expand("lib_mce[${MAS_CARD}]", c->fibre_card);
#else
  ptr = strdup("lib_mce");
#endif
  c->maslog = maslog_connect(c, ptr);
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
  config_destroy(context->mas_cfg);
  free(context->mas_cfg);

  free(context);
}


char* mcelib_version()
{
  return VERSION_STRING;
}
