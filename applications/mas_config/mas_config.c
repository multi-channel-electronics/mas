/* -*- mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *      vim: sw=4 ts=4 et tw=80
 */
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <getopt.h>
#include <mce/defaults.h>
#include <mce_library.h>

#include "../../defaults/config.h"

#define OPT_SH_ENV  1000
#define OPT_CSH_ENV 1001
#define OPT_HELP    1002
typedef enum { OPT_VERSION = OPT_HELP + 1, OPT_PREFIX,
    OPT_LIBS, OPT_CFLAGS, OPT_MASFILE, OPT_HARDWARE_FILE, OPT_EXPERIMENT_FILE,
    OPT_MCE_DEVNUM, OPT_BIGPHYS, OPT_FAKEMCE, OPT_MAX_FIBRE_CARD, OPT_USER,
    OPT_GROUP, OPT_MAS_DATA, OPT_MAS_DATA_ROOT, OPT_MAS_TEMPLATE, OPT_MAS_BIN,
    OPT_MAS_TEMP, OPT_MAS_SCRIPT, OPT_MAS_IDL, OPT_MAS_PYTHON, OPT_MAS_ROOT,
    OPT_MAS_TEST_SUITE, OPT_PATH_BASE, OPT_PATH, OPT_PYTHONPATH,
    OPT_PYTHONPATH_BASE, OPT_NUM_MCE, OPT_MCE_DEVNAME, OPT_MAS_ETC } parm_t;

#define OPT_DEFAULT(x) \
    case OPT_MAS_ ## x: \
    puts(mcelib_lookup_dir(mce, MAS_DIR_ ## x)); \
    break

static void __attribute__ ((format (printf, 3, 4))) say_env(int csh, const char *name, const char *style, ...)
{
    va_list ap;
    char *fmt = malloc(strlen(style) + strlen(name) + 13);

    va_start(ap, style);

    if (csh)
        sprintf(fmt, "setenv %s \"%s\";\n", name, style);
    else
        sprintf(fmt, "export %s=\"%s\";\n", name, style);

    vprintf(fmt, ap);
    va_end(ap);
    free(fmt);
}

static char *strip_path(const mce_context_t mce, const char *var)
{
    char *path_in = getenv(var);
    const char *mas_root = mcelib_lookup_dir(mce, MAS_DIR_ROOT);
    size_t mas_root_len = 0;
    char *path_out;
    char *elem;

    if (path_in == NULL || path_in[0] == 0)
        return strdup("");

    if (mas_root)
        mas_root_len = strlen(mas_root);

    path_out = malloc(strlen(path_in) + 1);
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

static void setup_env(int devnum, const mce_context_t mce, int csh)
{
    char *path, *base;

    const char *mas_bin;
    const char *mas_python;
    const char *mas_script;
    const char *mas_test_suite;

    /* device number */
    if (devnum == -1) 
        say_env(csh, "MAS_MCE_DEV", "%i", mcelib_default_mce());
    else
        say_env(csh, "MAS_MCE_DEV", "%i", devnum);
    say_env(csh, "MAS_MCE_NDEV", "%i", mcelib_ndev(mce));
    say_env(csh, "MAS_MCE_DEVNAME", "%s", mcelib_dev(mce));

    say_env(csh, "MAS_PREFIX", "%s", MAS_PREFIX);
    say_env(csh, "MAS_ROOT", "%s", mcelib_lookup_dir(mce, MAS_DIR_ROOT));
    puts("");

    mas_bin = mcelib_lookup_dir(mce, MAS_DIR_BIN);
    say_env(csh, "MAS_BIN", "%s", mas_bin);
    say_env(csh, "MAS_TEMP", "%s", mcelib_lookup_dir(mce, MAS_DIR_TEMP));
    say_env(csh, "MAS_DATA_ROOT", "%s", mcelib_lookup_dir(mce, MAS_DIR_DATA_ROOT));
    say_env(csh, "MAS_DATA", "%s", mcelib_lookup_dir(mce, MAS_DIR_DATA));
    puts("");

    mas_script = mcelib_lookup_dir(mce, MAS_DIR_SCRIPT);
    mas_python = mcelib_lookup_dir(mce, MAS_DIR_PYTHON);
    mas_test_suite = mcelib_lookup_dir(mce, MAS_DIR_TEST_SUITE);

    say_env(csh, "MAS_IDL", "%s", mcelib_lookup_dir(mce, MAS_DIR_IDL));
    say_env(csh, "MAS_PYTHON", "%s", mas_python);
    say_env(csh, "MAS_SCRIPT", "%s", mas_script);
    say_env(csh, "MAS_TEMPLATE", "%s", mcelib_lookup_dir(mce, MAS_DIR_TEMPLATE));
    say_env(csh, "MAS_TEST_SUITE", "%s", mas_test_suite);
    puts("");


    base = strip_path(mce, "PATH");
    path = malloc(strlen(base) + strlen(mas_bin) + strlen(mas_script) + strlen(mas_test_suite) + 4);
    if (base[0] == 0)
        sprintf(path, "%s:%s:%s", mas_bin, mas_script, mas_test_suite);
    else
        sprintf(path, "%s:%s:%s:%s", base, mas_bin, mas_script, mas_test_suite);
    say_env(csh, "PATH", "%s", path);
    free(base);
    free(path);

    base = strip_path(mce, "PYTHONPATH");
    path = malloc(strlen(base) + strlen(mas_python) + sizeof(MAS_PREFIX) + 9);
    if (base[0] == 0)
        sprintf(path, "%s:" MAS_PREFIX "/python", mas_python);
    else
        sprintf(path, "%s:%s:" MAS_PREFIX "/python", base, mas_python);
    say_env(csh, "PYTHONPATH", "%s", path);
    free(base);
    free(path);
}

void __attribute__((noreturn)) Usage(int ret)
{
    printf("Usage: mas_config [OPTION]... [PARAMETER]...\n"
            "\nOptions:\n"
            "  -c                generate C-shell commands on stdout for "
            "regularising the\n"
            "                      environment for the current MCE device\n"
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
            "implicitly true\n"
            "  -s                generate Bourne shell commands on stdout for "
            "regularising\n"
            "                      the environment for the current MCE device\n"
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
            "described above\n"
            "  --data-dir        the current data directory (MAS_DATA in the"
            "environment)\n"
            "  --data-root       the root data directory (MAS_DATA_ROOT in the "
            "environment)\n"
            "  --etc-dir         the directory containing the hardware "
            "description files\n"
            "                      (MAS_ETC in the environment.)\n"
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
            "  --mas-root        the base directory for the MCE script tree "
            "(MAS_ROOT\n"
            "                      in the environment\n"
            "  --max-fibre-card  the maximum number of fibre cards supported "
            "by the\n"
            "                      kernel driver\n"
            "  --mce-devname     the name of the current MCE device\n"
            "  --mce-devnum      the number of the current MCE device\n"
            "  --num-mce         the number number of MCEs managed by MAS\n"
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
            "MAS root\n"
            "                      (see --mas-root).  The value is suitable "
            "for providing\n"
            "                      to --path\n"
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
            "--prefix) or MAS\n"
            "                      root (see --mas-root).  The value is "
            "suitable for\n"
            "                      providing to --pythonpath\n"
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
            "\nIf -c, -q, and -s are not specified, multiple parameters may be "
            "given.  The\nvalue of each parameter will be reported on standard "
            "output, one per line, in\nthe order specified.\n");

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
        { "etc-dir", 0, NULL, OPT_MAS_ETC },
        { "experiment-file", 0, NULL, OPT_EXPERIMENT_FILE },
        { "mce-devname", 0, NULL, OPT_MCE_DEVNAME },
        { "mce-devnum", 0, NULL, OPT_MCE_DEVNUM },
        { "bigphys", 0, NULL, OPT_BIGPHYS },
        { "fakemce", 0, NULL, OPT_FAKEMCE },
        { "max-fibre-card", 0, NULL, OPT_MAX_FIBRE_CARD },
        { "num-mce", 0, NULL, OPT_NUM_MCE },
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
        { "mas-root", 0, NULL, OPT_MAS_ROOT },
        { NULL, 0, NULL, 0 }
    };
    int option, devnum = -1;
    parm_t plist[MAX_PARAM];
    const char *parg[MAX_PARAM];
    int i, np = 0;
    int boolean = 0;
    int do_env = -1;
    char *ptr, *ptr_in;
    char *mas_cfg = NULL;
    const char *mas_path1;
    const char *mas_path2;
    const char *mas_path3;

    while ((option = getopt_long(argc, argv, "cm:n:qs", opts, NULL)) >=0) {
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
        } else if (option == 's')
          do_env = 0;
        else if (option == 'c')
          do_env = 1;
        else if (option == '?')
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
    if (np == 0 && do_env == -1) {
        printf("No parameter specified.\n");
        Usage(0);
    }

    /* quiet boolean */
    if (do_env != -1) {
        mce_context_t mce = mcelib_create(devnum, mas_cfg, 0);
        if (mce == NULL)
            return 1;
        setup_env(devnum, mce, do_env);
        return 0;
    } else if (boolean) {
        switch (plist[0]) {
            case OPT_BIGPHYS:
#if BIGPHYS
                return 0;
#else
                return 1;
#endif
            case OPT_FAKEMCE:
#if FAKEMCE
                return 0;
#else
                return 1;
#endif
            default:
                return 0;
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
                ptr = mcelib_default_hardwarefile(mce);
                puts(ptr);
                free(ptr);
                break;
            case OPT_EXPERIMENT_FILE:
                ptr = mcelib_default_experimentfile(mce);
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
            case OPT_NUM_MCE:
                printf("%i\n", mcelib_ndev(mce));
                break;
            case OPT_USER:
                puts(MAS_USER);
                break;
            case OPT_GROUP:
                puts(MAS_GROUP);
                break;
            case OPT_PATH_BASE:
                ptr = strip_path(mce, "PATH");
                puts(ptr);
                free(ptr);
                break;
            case OPT_PATH:
                if (parg[i]) {
                    setenv("PATH", parg[i], 1);
                }

                mas_path1 = mcelib_lookup_dir(mce, MAS_DIR_BIN);
                mas_path2 = mcelib_lookup_dir(mce, MAS_DIR_SCRIPT);
                mas_path3 = mcelib_lookup_dir(mce, MAS_DIR_TEST_SUITE);

                ptr_in = malloc(11 + strlen(mas_path1) + strlen(mas_path2) + strlen(mas_path3));

                if (getenv("PATH") == NULL)
                    sprintf(ptr_in, "%s:%s:%s", mas_path1, mas_path2, mas_path3);
                else
                    sprintf(ptr_in, "${PATH}:%s:%s:%s", mas_path1, mas_path2, mas_path3);

                ptr = mcelib_shell_expand(mce, ptr_in);
                puts(ptr);
                free(ptr_in);
                free(ptr);
                break;
            case OPT_PYTHONPATH_BASE:
                ptr = strip_path(mce, "PYTHONPATH");
                puts(ptr);
                free(ptr);
                break;
            case OPT_PYTHONPATH:
                if (parg[i])
                    setenv("PYTHONPATH", parg[i], 1);

                mas_path1 = mcelib_lookup_dir(mce, MAS_DIR_PYTHON);

                ptr_in = malloc(23 + sizeof(MAS_PREFIX) + strlen(mas_path1));

                if (getenv("PYTHONPATH") == NULL)
                    sprintf(ptr_in, "%s:" MAS_PREFIX "/python", mas_path1);
                else
                    sprintf(ptr_in, "${PYTHONPATH}:%s:" MAS_PREFIX "/python", mas_path1);

                ptr = mcelib_shell_expand(mce, ptr_in);
                puts(ptr);
                free(ptr_in);
                free(ptr);
                break;

                OPT_DEFAULT(BIN);
                OPT_DEFAULT(DATA);
                OPT_DEFAULT(DATA_ROOT);
                OPT_DEFAULT(ETC);
                OPT_DEFAULT(IDL);
                OPT_DEFAULT(PYTHON);
                OPT_DEFAULT(ROOT);
                OPT_DEFAULT(SCRIPT);
                OPT_DEFAULT(TEMP);
                OPT_DEFAULT(TEMPLATE);
                OPT_DEFAULT(TEST_SUITE);
        }
    }

    return 0;
}
