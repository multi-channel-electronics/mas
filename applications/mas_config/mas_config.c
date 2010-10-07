#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <mce/defaults.h>

#include "../../defaults/config.h"

#define OPT_HELP 1000
typedef enum { OPT_VERSION = OPT_HELP + 1, OPT_MULTICARD, OPT_PREFIX,
  OPT_LIBS, OPT_CFLAGS, OPT_MASFILE, OPT_HARDWARE_FILE, OPT_EXPERIMENT_FILE,
  OPT_FIBRE_CARD, OPT_CMD_DEVICE, OPT_DATA_DEVICE, OPT_DSP_DEVICE,
  OPT_BIGPHYS, OPT_FAKEMCE, OPT_MAX_FIBRE_CARD, OPT_USER, OPT_GROUP} parm_t;

void __attribute__((noreturn)) Usage(int ret)
{
  printf("Usage: mas_config [OPTION]... [PARAMETER]...\n"
      "\nOptions:\n"
#if MULTICARD
      "  -n <card>         report values for the specified fibre card\n"
#else
      "  -n <card>         ignored\n"
#endif
      "  -q                instead of reporting the value on standard output, "
      "exit with\n"
      "                      non-zero status if the first parameter is false, "
      "otherwise\n"
      "                      exit with zero status.  Only makes sense with a "
      "boolean\n"
      "                      parameter: all other parameters are implicitly "
      "true.\n"
      "  --help            display this help and exit\n"
      "\nParameters:\n"
      "  --bigphys         boolean; true if the driver supports bigphysarea\n"
      "  --cflags          cc(1) options needed to complile against the MAS "
      "libraries\n"
      "  --cmd-device      the full path of the MCE command device\n"
      "  --config-file     the full path of the MAS configuration file\n"
      "  --data-device     the full path of the MCE data device\n"
      "  --dsp-device      the full path of the MCE dsp device\n"
      "  --experiment-file the full path of the experiment configuration file\n"
      "  --fakemce         boolean; true if the driver contains the fakemce device\n"
#if MULTICARD
      /* this is still accepted by single card MAS, we just don't mention it
       * because it's non-informative */
      "  --fibre-card      the number of the current fibre card\n"
#endif
      "  --group           the MAS group\n"
      "  --hardware-file   the full path of the hardware configuration file\n"
      "  --libs            ld(1) options needed to link to the MAS libraries\n"
      "  --max-fibre-card  the maximum number of fibre cards supported\n"
      "  --multicard       booelan; true if MAS supports multiple fibre cards\n"
      "  --prefix          the MAS prefix\n"
      "  --user            the MAS username\n"
      "  --version         the mas_config version\n"
      "\nIf -q is not specified, multiple parameters may be given.  The value "
      "of each\nparameter will be reported on standard output a separate line, "
      "in the order\nspecified.\n");

  exit(ret);
}

#define MAX_PARAM 100
int main(int argc, char **argv)
{
  const struct option opts[] = {
    { "help", 0, NULL, OPT_HELP },
    { "version", 0, NULL, OPT_VERSION },
    { "multicard", 0, NULL, OPT_MULTICARD },
    { "prefix", 0, NULL, OPT_PREFIX },
    { "libs", 0, NULL, OPT_LIBS },
    { "cflags", 0, NULL, OPT_CFLAGS },
    { "config-file", 0, NULL, OPT_MASFILE },
    { "hardware-file", 0, NULL, OPT_HARDWARE_FILE },
    { "experiment-file", 0, NULL, OPT_EXPERIMENT_FILE },
    { "fibre-card", 0, NULL, OPT_FIBRE_CARD },
    { "cmd-device", 0, NULL, OPT_CMD_DEVICE },
    { "data-device", 0, NULL, OPT_DATA_DEVICE },
    { "dsp-device", 0, NULL, OPT_DSP_DEVICE },
    { "bigphys", 0, NULL, OPT_BIGPHYS },
    { "fakemce", 0, NULL, OPT_FAKEMCE },
    { "max-fibre-card", 0, NULL, OPT_MAX_FIBRE_CARD },
    { "user", 0, NULL, OPT_USER },
    { "group", 0, NULL, OPT_GROUP },
    { NULL, 0, NULL, 0 }
  };
  int option, fibre_card = -1;
  parm_t plist[MAX_PARAM];
  int i, np = 0;
  int boolean = 0;
  char *ptr;

  while ((option = getopt_long(argc, argv, "n:q", opts, NULL)) >=0) {
    if (option == 'n') {
#if MULTICARD
        fibre_card = (int)strtol(optarg, &ptr, 10);
        if (*optarg == '\0' || *ptr != '\0' || fibre_card < 0) {
          fprintf(stderr, "%s: invalid fibre card number\n", argv[0]);
          return -1;
        }
#endif
    } else if (option == 'q') 
      boolean = 1;
    else if (option == '?')
      Usage(1);
    else if (option == OPT_HELP)
      Usage(0);
    else { /* parameters */
      if (np < MAX_PARAM)
        plist[np++] = (parm_t)option;
    }
  }

  /* nothing to do */
  if (np == 0) {
    printf("No parameter specified.\n");
    Usage(0);
  }

  /* quiet boolean */
  if (boolean) {
    switch (plist[0]) {
      case OPT_MULTICARD:
#if MULTICARD
        exit(0);
#else
        exit(1);
#endif
      case OPT_BIGPHYS:
#if BIGPHYS
        exit(0);
#else
        exit(1);
#endif
      case OPT_FAKEMCE:
#if FAKEMCE
        exit(0);
#else
        exit(1);
#endif
      default:
        exit(0);
    }
  }

  /* otherwise, print all values */
  for (i = 0; i < np; ++i) {
    switch(plist[i]) {
      case OPT_VERSION:
        printf("%s\n", VERSION_STRING);
        break;
      case OPT_MULTICARD:
#if MULTICARD
        puts("True");
#else
        puts("False");
#endif
        break;
      case OPT_PREFIX:
        printf("%s\n", MAS_PREFIX);
        break;
      case OPT_LIBS:
#ifdef SHARED
        printf("-L%s/lib -lmcedsp -lmcecmd -lmaslog\n", MAS_PREFIX);
#else
        printf("-L%s/lib -lmcedsp -lmcecmd -lmaslog -lconfig -lpthread\n",
            MAS_PREFIX);
#endif
        break;
      case OPT_CFLAGS:
        printf("-I%s/include\n", MAS_PREFIX);
        break;
      case OPT_MASFILE:
        ptr = mcelib_default_masfile();
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_HARDWARE_FILE:
        ptr = mcelib_default_hardwarefile(fibre_card);
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_EXPERIMENT_FILE:
        ptr = mcelib_default_experimentfile(fibre_card);
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_FIBRE_CARD:
#if MULTICARD
        if (fibre_card == -1)
          printf("%i\n", mcelib_default_fibre_card());
        else
          printf("%i\n", fibre_card);
#else
        puts("0");
#endif
        break;
      case OPT_CMD_DEVICE:
        ptr = mcelib_cmd_device(fibre_card);
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_DATA_DEVICE:
        ptr = mcelib_data_device(fibre_card);
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_DSP_DEVICE:
        ptr = mcelib_dsp_device(fibre_card);
        printf("%s\n", ptr);
        free(ptr);
        break;
      case OPT_BIGPHYS:
#if BIGPHYS
        puts("True");
#else
        puts("False");
#endif
        break;
      case OPT_FAKEMCE:
#if FAKEMCE
        puts("True");
#else
        puts("False");
#endif
        break;
      case OPT_MAX_FIBRE_CARD:
#if MULTICARD
        printf("%i\n", MAX_FIBRE_CARD);
#else
        puts("1");
#endif
        break;
      case OPT_USER:
        printf("%s\n", MAS_USER);
        break;
      case OPT_GROUP:
        printf("%s\n", MAS_GROUP);
        break;
    }
  }

  return 0;
}
