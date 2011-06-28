/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <mce/defaults.h>
#include <mce_library.h>

#include "../../defaults/config.h"

#define OPT_HELP 1000
typedef enum { OPT_VERSION = OPT_HELP + 1, OPT_PREFIX,
    OPT_LIBS, OPT_CFLAGS, OPT_MASFILE, OPT_HARDWARE_FILE, OPT_EXPERIMENT_FILE,
    OPT_MCE_DEVNUM, OPT_BIGPHYS, OPT_FAKEMCE, OPT_MAX_FIBRE_CARD, OPT_USER,
    OPT_GROUP, OPT_MAS_DATA, OPT_MAS_DATA_ROOT, OPT_MAS_TEMPLATE, OPT_MAS_BIN,
    OPT_MAS_TEMP, OPT_MAS_SCRIPT, OPT_MAS_IDL, OPT_MAS_PYTHON,
    OPT_MAS_TEST_SUITE, OPT_PATH_BASE, OPT_PATH, OPT_PYTHONPATH,
    OPT_PYTHONPATH_BASE, OPT_MAX_MCE, OPT_MCE_DEVNAME } parm_t;

#define OPT_DEFAULT(x) \
        case OPT_MAS_ ## x: \
    ptr = mcelib_shell_expand("${MASDEFAULT_" #x "}", devnum); \
    puts(ptr); \
    free(ptr); \
    break

static char *strip_path(const char *var)
{
    char *path_in = getenv(var);
    char *mas_root = getenv("MAS_ROOT");
    size_t mas_root_len = 0;
    char *path_out;
    char *elem;

    if (path_in == NULL || path_in[0] == 0)
        return strdup("");

    if (mas_root)
        mas_root_len = strlen(mas_root);

    path_out = malloc(strlen(path_in));
    path_out[0] = 0;

    for (elem = strtok(path_in, ":"); elem != NULL; elem = strtok(NULL, ":")) {
        if (strncmp(elem, MAS_PREFIX, sizeof(MAS_PREFIX) - 1) == 0)
            ; /* found prefix */
        else if (mas_root && strncmp(elem, mas_root, mas_root_len) == 0)
            ; /* found MAS_ROOT */
        else
            strcat(strcat(path_out, elem), ":");
    }

    /* strip trailing : */
    path_out[strlen(path_out) - 1] = 0;

    return path_out;
}

void __attribute__((noreturn)) Usage(int ret)
{
    printf("Usage: mas_config [OPTION]... [PARAMETER]...\n"
            "\nOptions:\n"
            "  -m <file>         read MAS configuration from the specified "
            "file instead of\n"
            "                      the default mas.cfg (the one reported by "
            "--config-file)\n"
            "  -n <mce#>         report values for the specified MCE device\n"
            "  -q                instead of reporting the value on standard "
            "output, exit with\n"
            "                      non-zero status if the first parameter is "
            "false, otherwise\n"
            "                      exit with zero status.  Only makes sense "
            "with a boolean\n"
            "                      parameter: all other parameters are "
            "implicitly true.\n"
            "  --help            display this help and exit\n"
            "\nParameters:\n"
            "  --bigphys         boolean; true if the driver supports "
            "bigphysarea\n"
            "  --bin-dir         the location of the MAS binaries (MAS_BIN in "
            "the\n"
            "                      environment)\n"
            "  --cflags          cc(1) options needed to complile against the "
            "MAS libraries\n"
            "  --config-file     the full path of the MAS configuration file "
            "(mas.cfg).  This\n"
            "                      value is not affected by the -m option "
            "described above.\n"
            "  --data-dir        the current data directory (MAS_DATA in the"
            "environment)\n"
            "  --data-root       the root data directory (MAS_DATA_ROOT in the "
            "environment)\n"
            "  --experiment-file the full path of the experiment configuration "
            "file\n"
            "                      (experiment.cfg)\n"
            "  --fakemce         boolean; true if the driver contains the "
            "fakemce device\n"
            "  --group           the MAS group\n"
            "  --hardware-file   the full path of the hardware configuration "
            "file (mce.cfg)\n"
            "  --idl-dir         the MCE script IDL directory (MAS_IDL in the "
            "environment)\n"
            "  --libs            ld(1) options needed to link to the MAS "
            "libraries\n"
            "  --max-fibre-card  the maximum number of fibre cards supported "
            "by the\n"
            "                      kernel driver\n"
            "  --max-mce         the maximum number of MCEs managed by MAS.\n"
            "  --mce-devname     the name of the current MCE device.\n"
            "  --mce-devnum      the number of the current MCE device.\n"
            "  --path[=PATHBASE] a shell PATH variable for MAS.  If given, MAS "
            "specific\n"
            "                      directories will be appended to PATHBASE, "
            "otherwise they\n"
            "                      will be appended to the current PATH.  (See "
            "also\n"
            "                      --path-base below.)\n"
            "  --path-base       the current PATH variable stripped of any "
            "directories which\n"
            "                      start with the MAS prefix (see --prefix) or "
            "the MAS_ROOT\n"
            "                      environment variable, if it is defined.  "
            "The value is\n"
            "                      suitable for providing to --path.\n"
            "  --prefix          the MAS prefix\n"
            "  --pythonpath[=PATHBASE]\n"
            "                    a PYTHONPATH variable for MAS.  If given, MAS "
            "specific\n"
            "                      directories will be appended to PATHBASE, "
            "otherwise they\n"
            "                      will be appended to the current "
            "PYTHONPATH.  (See also\n"
            "                      --pythonpath-base below.)\n"
            "  --pythonpath-base the current PYTHONPATH variable stripped of "
            "any directories\n"
            "                      which start with the MAS prefix (see "
            "--prefix) or the\n"
            "                      MAS_ROOT environment variable, if it is "
            "defined.  The\n"
            "                      value is suitable for providing to "
            "--pythonpath.\n"
            "  --python-dir      the MCE script python directory (MAS_PYTHON "
            "in the\n"
            "                      environment)\n"
            "  --script-dir      the MCE script directory (MAS_SCRIPT in the "
            "environment)\n"
            "  --temp-dir        the MAS temporary directory (MAS_TEMP in the "
            "environment)\n"
            "  --template-dir    the MAS template directory (MAS_TEMPLATE in "
            "the environment)\n"
            "  --test-suite      the MAS test suite directory (MAS_TEST_SUITE "
            "in the\n"
            "                      environment)\n"
            "  --user            the MAS username\n"
            "  --version         the mas_config version\n"
            "\nIf -q is not specified, multiple parameters may be given.  The "
            "value of each\nparameter will be reported on standard output, one "
            "per line, in the order\nspecified.\n");

    exit(ret);
}

#define MAX_PARAM 100
int main(int argc, char **argv)
{
    const struct option opts[] = {
        { "help", 0, NULL, OPT_HELP },
        { "version", 0, NULL, OPT_VERSION },
        { "prefix", 0, NULL, OPT_PREFIX },
        { "libs", 0, NULL, OPT_LIBS },
        { "cflags", 0, NULL, OPT_CFLAGS },
        { "config-file", 0, NULL, OPT_MASFILE },
        { "hardware-file", 0, NULL, OPT_HARDWARE_FILE },
        { "experiment-file", 0, NULL, OPT_EXPERIMENT_FILE },
        { "mce-devname", 0, NULL, OPT_MCE_DEVNAME },
        { "mce-devnum", 0, NULL, OPT_MCE_DEVNUM },
        { "bigphys", 0, NULL, OPT_BIGPHYS },
        { "fakemce", 0, NULL, OPT_FAKEMCE },
        { "max-fibre-card", 0, NULL, OPT_MAX_FIBRE_CARD },
        { "max-mce", 0, NULL, OPT_MAX_MCE },
        { "user", 0, NULL, OPT_USER },
        { "group", 0, NULL, OPT_GROUP },
        { "data-dir", 0, NULL, OPT_MAS_DATA },
        { "data-root", 0, NULL, OPT_MAS_DATA_ROOT },
        { "template-dir", 0, NULL, OPT_MAS_TEMPLATE },
        { "bin-dir", 0, NULL, OPT_MAS_BIN },
        { "temp-dir", 0, NULL, OPT_MAS_TEMP },
        { "script-dir", 0, NULL, OPT_MAS_SCRIPT },
        { "idl-dir", 0, NULL, OPT_MAS_IDL },
        { "python-dir", 0, NULL, OPT_MAS_PYTHON },
        { "test-suite", 0, NULL, OPT_MAS_TEST_SUITE },
        { "path", 2, NULL, OPT_PATH },
        { "path-base", 0, NULL, OPT_PATH_BASE },
        { "pythonpath", 2, NULL, OPT_PYTHONPATH },
        { "pythonpath-base", 0, NULL, OPT_PYTHONPATH_BASE },
        { NULL, 0, NULL, 0 }
    };
    int option, devnum = -1;
    parm_t plist[MAX_PARAM];
    const char *parg[MAX_PARAM];
    int i, np = 0;
    int boolean = 0;
    char *ptr;
    char *mas_cfg = NULL;

    while ((option = getopt_long(argc, argv, "m:n:q", opts, NULL)) >=0) {
        if (option == 'n') {
            devnum = (int)strtol(optarg, &ptr, 10);
            if (*optarg == '\0' || *ptr != '\0' || devnum < 0) {
                fprintf(stderr, "%s: invalid fibre card number\n", argv[0]);
                return -1;
            }
        } else if (option == 'q')
            boolean = 1;
        else if (option == 'm') {
            if (mas_cfg)
                free(mas_cfg);
            mas_cfg = strdup(optarg);
        } else if (option == '?')
            Usage(1);
        else if (option == OPT_HELP)
            Usage(0);
        else { /* parameters */
            if (np < MAX_PARAM) {
                if (optarg)
                    parg[np] = strdup(optarg);
                else
                    parg[np] = NULL;
                plist[np++] = (parm_t)option;
            }
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

    /* create a library context */
    mce_context_t mce = mcelib_create(devnum, mas_cfg, 0);
    if (mce == NULL)
        return 1;

    /* ensure MAS_PREFIX is correct */
    setenv("MAS_PREFIX", MAS_PREFIX, 1);

    /* otherwise, print all values */
    for (i = 0; i < np; ++i) {
        switch(plist[i]) {
            case OPT_VERSION:
                puts(VERSION_STRING);
                break;
            case OPT_PREFIX:
                puts(MAS_PREFIX);
                break;
            case OPT_LIBS:
#ifdef SHARED
                printf("-L%s/lib -lmcedsp -lmce\n", MAS_PREFIX);
#else
                printf("-L%s/lib -lmcedsp -lmce %s\n", MAS_PREFIX,
                        MCE_STATIC_LIBS);
#endif
                break;
            case OPT_CFLAGS:
                printf("-I%s/include\n", MAS_PREFIX);
                break;
            case OPT_MASFILE:
                ptr = mcelib_default_masfile();
                puts(ptr);
                free(ptr);
                break;
            case OPT_HARDWARE_FILE:
                ptr = mcelib_default_hardwarefile(devnum);
                puts(ptr);
                free(ptr);
                break;
            case OPT_EXPERIMENT_FILE:
                ptr = mcelib_default_experimentfile(devnum);
                puts(ptr);
                free(ptr);
                break;
            case OPT_MCE_DEVNAME:
                printf("%s\n", mcelib_dev(mce));
                break;
            case OPT_MCE_DEVNUM:
                if (devnum == -1)
                    printf("%i\n", mcelib_default_mce());
                else
                    printf("%i\n", devnum);
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
                printf("%i\n", MAX_FIBRE_CARD);
                break;
            case OPT_MAX_MCE:
                printf("%i\n", mcelib_ndev(mce));
                break;
            case OPT_USER:
                puts(MAS_USER);
                break;
            case OPT_GROUP:
                puts(MAS_GROUP);
                break;
            case OPT_PATH_BASE:
                ptr = strip_path("PATH");
                puts(ptr);
                free(ptr);
                break;
            case OPT_PATH:
                if (parg[i]) {
                    setenv("PATH", parg[i], 1);
                }

                ptr = mcelib_shell_expand("${PATH}:${MAS_BIN}:${MAS_SCRIPT}:"
                        "${MAS_TEST_SUITE}", -1);
                puts(ptr);
                free(ptr);
                break;
            case OPT_PYTHONPATH_BASE:
                ptr = strip_path("PYTHONPATH");
                puts(ptr);
                free(ptr);
                break;
            case OPT_PYTHONPATH:
                if (parg[i])
                    setenv("PYTHONPATH", parg[i], 1);

                ptr = mcelib_shell_expand("${PYTHONPATH}:${MAS_PYTHON}:"
                        MAS_PREFIX "/python", -1);
                puts(ptr);
                free(ptr);
                break;
            case OPT_MAS_DATA:
                /* set data root, if necessary */
                ptr = mcelib_shell_expand("${MASDEFAULT_DATA_ROOT}",
                        devnum);
                setenv("MAS_DATA_ROOT", ptr, 0);
                free(ptr);

                ptr = mcelib_shell_expand("${MASDEFAULT_DATA}", devnum);
                puts(ptr);
                free(ptr);
                break;

                OPT_DEFAULT(BIN);
                OPT_DEFAULT(DATA_ROOT);
                OPT_DEFAULT(IDL);
                OPT_DEFAULT(PYTHON);
                OPT_DEFAULT(SCRIPT);
                OPT_DEFAULT(TEMP);
                OPT_DEFAULT(TEMPLATE);
                OPT_DEFAULT(TEST_SUITE);
        }
    }

    return 0;
}
