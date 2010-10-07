/* Helper routines for paths &c. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wordexp.h>
#include "../../defaults/config.h"

/* sanitise the environment */
static void check_env(int card)
{
  static int done = -1;

#if !MULTICARD
  card = 0;
#else
  if (card < 0 || card >= MAX_FIBRE_CARD)
    card = -1;
#endif
  
  /* no need to try this every time */
  if ((card != -1 && done == card) ||
      (card == -1 && done == DEFAULT_FIBRE_CARD))
  {
    return;
  }

#if !MULTICARD
  setenv("MAS_CARD", "0", 1);
  setenv("MAS_DATA", "/data/cryo/current_data", 0);
#else
  /* MAS_CARD is overridden if card is set */
  char buffer[10];
  if (card == -1) {
    sprintf(buffer, "%i", DEFAULT_FIBRE_CARD);
    setenv("MAS_CARD", buffer, 0);
    card = DEFAULT_FIBRE_CARD;
  } else {
    sprintf(buffer, "%i", card);
    setenv("MAS_CARD", buffer, 1);
  }
  setenv("MAS_DATA", "/data/cryo${MAS_CARD}/current_data", 0);
#endif
  done = card;
}

/* shell expand input; returns a newly malloc'd string on success or
 * NULL on error */
char *mcelib_shell_expand(const char* input, int card)
{
#if 0
  fprintf(stderr, "%s(\"%s\", %i)\n", __func__, input, card);
#endif
  char *ptr;
  static int depth = 0;
  wordexp_t expansion;

  /* avoid infinite recursion */
  if (depth > 100) {
    ptr = strdup(input);
    return ptr;
  } else
    depth++;

  check_env(card);

  /* shell expand the path, if necessary */
  if (wordexp(input, &expansion, 0) != 0) {
    /* path expansion failed */
    depth--;
    return NULL;
  }

  if (expansion.we_wordc > 1) {
    /* path expansion resulted in multiple values */
    depth--;
    return NULL;
  }

  ptr = strdup(expansion.we_wordv[0]);

  /* clean up */
  wordfree(&expansion);
  
  /* if there are still things to expand, run it through again */
  if (strchr(ptr, '$')) {
    char *old = ptr;
    ptr = mcelib_shell_expand(ptr, card);
    free(old);
  }
#if 0
  else
    fprintf(stderr, "%s = \"%s\"\n", __func__, ptr);
#endif

  depth--;
  return ptr;
}

int mcelib_default_fibre_card(void)
{
#if !MULTICARD
  return 0;
#else
  const char *ptr = getenv("MAS_CARD");
  return (ptr == NULL) ?  DEFAULT_FIBRE_CARD : atoi(ptr);
#endif
}

/* generic path expansionny, stuff.  The caller must check that the
 * pointer returned by these functions isn't NULL. */
char *mcelib_default_experimentfile(int card)
{
  return mcelib_shell_expand(DEFAULT_EXPERIMENTFILE, card);
}

char *mcelib_default_hardwarefile(int card)
{
  return mcelib_shell_expand(DEFAULT_HARDWAREFILE, card);
}

char *mcelib_default_masfile(void)
{
  return mcelib_shell_expand(DEFAULT_MASFILE, -1);
}

/* device node names */
char *mcelib_cmd_device(int card)
{
  return mcelib_shell_expand(DEFAULT_CMD_DEVICE, card);
}

char *mcelib_data_device(int card)
{
  return mcelib_shell_expand(DEFAULT_DATA_DEVICE, card);
}

char *mcelib_dsp_device(int card)
{
  return mcelib_shell_expand(DEFAULT_DSP_DEVICE, card);
}
