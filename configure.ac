dnl get git revision for AC_INIT
m4_define(branch_id_val, sinclude(branch_id.txt))
m4_define(repo_version, [esyscmd(./get_repo_version.sh)])
m4_define(mas_version, [translit(branch_id_val/repo_version, [
])])

AC_INIT([MAS], [mas_version], [mhasse@phas.ubc.ca or dvw@phas.ubc.ca])

# Use branch_id.txt to verify the tree identity?
AC_CONFIG_SRCDIR([branch_id.txt])
AC_PREFIX_DEFAULT([/usr/mce])

dnl version stuff
REPO_VERSION=repo_version
AC_SUBST([REPO_VERSION])
AC_DEFINE_UNQUOTED([REPO_VERSION], ["${REPO_VERSION}"],
		[ The GIT repository version information ])
INT_REPO_VERSION=`echo ${REPO_VERSION} | sed -e 's/\(exported\|Unversioned\)/0/g; s/^\(@<:@0-9@:>@*\).*/\1/g'`
AC_DEFINE_UNQUOTED([INT_REPO_VERSION], [${INT_REPO_VERSION}],
		[The number of commits to this branch of the GIT repository])

BRANCH_ID=`cat branch_id.txt`
AC_DEFINE_UNQUOTED([BRANCH_ID], "${BRANCH_ID}",
		[ The MAS branch id ])
AC_SUBST([BRANCH_ID])

# We need C, and some stuff for installing.

AC_PROG_CC
AC_PROG_LN_S

dnl Allow the user to suppress doing the config2 stuff
AC_ARG_ENABLE([mce-cfg], [AS_HELP_STRING([--disable-mce-cfg],
              [disable building or installing mce.cfg file(s)])],
              [DO_CONFIG2=$enableval], [DO_CONFIG2=yes])

# Shared libraries
AC_ARG_ENABLE([shared], [AS_HELP_STRING([--enable-shared],
              [build shared libraries instead of static ones])],
              [if test "$enableval" = "no"; then SHARED="no"; else SHARED="yes";
               fi], [SHARED="no"])
AC_SUBST(SHARED)
if test "x$SHARED" = "xyes"; then
  AC_DEFINE([SHARED],, [ Define if building shared libraries ])
fi

# Allow the user to suppress driver compile / install.

AC_ARG_WITH([driver-version],
              [AS_HELP_STRING([--with-driver-version=VERSION],
                              [specifies which kernel module to compile.
                              TYPE should be one of: "c7", "c6", "none",
                              indicating, respectively: the new (U0107+)
                              driver, the old (U0106-) driver (this is the
                              default), or neither driver (disable
                              driver compilation).])],
            [
             case "${withval}" in
               (c7)     DRIVER_VERSION=c7 ;;
               (c6)     DRIVER_VERSION=c6 ;;
               (*)      DRIVER_VERSION=none ;;
             esac
             ], [DRIVER_VERSION=c6])

DO_C6_DRIVER=no
DO_C7_DRIVER=no
case "$DRIVER_VERSION" in
     (c6)    DO_C6_DRIVER=yes ;;
     (c7)    DO_C7_DRIVER=yes ;;
esac

# Rename the kernel module, if requested
AC_ARG_WITH([driver-name],
            [AS_HELP_STRING([--with-driver-name=NAME],
                            [call the compiled kernel module NAME; the
                             default is mce_dsp_XX, where XX is c6 or c7
                             depending on which driver is being built.])],
           [
            DRIVER_NAME=${withval}
            ], [DRIVER_NAME=mce_dsp_${DRIVER_VERSION}])
AC_SUBST([DRIVER_NAME])

dnl Allow disabling of the installation of the init script
AC_ARG_ENABLE([init-script],
              [AS_HELP_STRING([--disable-init-script], [don't install the ]dnl'
              [init script (/etc/init.d/mas).])],
              [
               case "${enableval}" in
                 (no) INSTALL_INITD_MAS=0 ;;
                 (*)  INSTALL_INITD_MAS=1 ;;
               esac
              ], [INSTALL_INITD_MAS=1])

dnl Allow user to disable the driver interface (for non-linux systems)
AC_ARG_ENABLE([mce-ops],
              [AS_HELP_STRING([--disable-mce-ops],
                              [disable userspace-kernel driver capabilities; ]
                              [implies "--disable-driver --disable-init-script ]
                              [--disable-mce-cfg" ]
                              [(useful for non-linux builds)])],
            [DO_KERNEL_IO=$enableval], [DO_KERNEL_IO=yes])

if test "x$DO_KERNEL_IO" != "xyes"; then
  INSTALL_INITD_MAS=no
  DO_C7_DRIVER=no
  DO_C6_DRIVER=no
  DO_CONFIG2=no
  DEFINE_NO_MCE_OPS="#define NO_MCE_OPS"
  MAKE_IF_MCE_OPS=#
else
  DEFINE_NO_MCE_OPS="#undef NO_MCE_OPS"
  MAKE_IF_MCE_OPS=
fi
AC_SUBST([MAKE_IF_MCE_OPS])
AC_SUBST([DEFINE_NO_MCE_OPS])
AC_SUBST([DO_CONFIG2])
AC_SUBST([DO_C6_DRIVER])
AC_SUBST([DO_C7_DRIVER])
AC_SUBST([INSTALL_INITD_MAS])

dnl Allow disabling of symlinks in /usr/local
AC_ARG_ENABLE([sys-links],
              [AS_HELP_STRING([--disable-sys-links], [don't create ]dnl'
              [symlinks in /usr/local pointing to the installed MAS libraries ]
              [and headers.  By default these links are created.])],
              [
               case "${enableval}" in
                 (no) INSTALL_SYS_LINKS=0 ;;
                 (*)  INSTALL_SYS_LINKS=1 ;;
               esac
              ], [INSTALL_SYS_LINKS=1])
AC_SUBST([INSTALL_SYS_LINKS])
                   
dnl Multicard MAS
AC_ARG_ENABLE([multicard],
              [AS_HELP_STRING([--enable-multicard=N], [build a MAS which ]
              [supports multiple (up to N) fibre cards.  If N is omitted, ]
              [2 is assumed.  N=0 and N=1 are eqivalent to --disable-multicard ]
              [ (ie. the default behaviour: multicard support disabled).])],
              [
               case "${enableval}" in
                 (0|1|no) MAX_FIBRE_CARD=1 ;;
                 (2|3|4|5|6|7|8|9) MAX_FIBRE_CARD="${enableval}" ;;
                 (*) MAX_FIBRE_CARD=2 ;;
               esac
               ], [MAX_FIBRE_CARD=1])
                   
AC_DEFINE_UNQUOTED(MAX_FIBRE_CARD, [$MAX_FIBRE_CARD],
          [ Define to the total allowable number of fibre cards. ])
AC_SUBST([MAX_FIBRE_CARD])
if test $MAX_FIBRE_CARD -eq 1; then
  FIRST_MCE_CIN="mce.cin"
  MCE_CFG_LIST="mce.cfg"
  MCE_CIN_LIST="mce.cin"
  MULTICARD=0
else
  FIRST_MCE_CIN="mce0.cin"
  MCE_CFG_LIST="mce0.cfg"
  MCE_CIN_LIST="mce0.cin"
  MULTICARD=1
  for n in $(seq $(expr $MAX_FIBRE_CARD - 1)); do
    MCE_CFG_LIST="$MCE_CFG_LIST mce$n.cfg"
    MCE_CIN_LIST="$MCE_CIN_LIST mce$n.cin"
  done
fi
AC_SUBST(MCE_CIN_LIST)
AC_SUBST(MCE_CFG_LIST)
AC_SUBST(FIRST_MCE_CIN)
AC_SUBST(MULTICARD)
AC_DEFINE_UNQUOTED(MULTICARD, [$MULTICARD],
                   [ Define to 1 to enable multicard MAS ])

dnl Some defaults.

AC_ARG_VAR([DEFAULT_MCE],
           [the default fibre card number (0 is used if not specified)])
if test -z "$DEFAULT_FIBRE_CARD"; then
  DEFAULT_MCE=0
fi
AC_DEFINE_UNQUOTED([DEFAULT_MCE], [$DEFAULT_MCE],
                   [ Define to the number of the default fibre card. ])

# Some driver options.

AC_ARG_ENABLE([quiet-driver], [AS_HELP_STRING([--enable-quiet-driver],
      [build an unnecessarily quiet kernel driver])],
    [if test "x$withval" = "xno"; then
   DRIVER_QUIET=no
else
   DRIVER_QUIET=yes
fi],
[DRIVER_QUIET=no])

if test "x$DRIVER_QUIET" = "xyes"; then
  AC_DEFINE(DRIVER_QUIET, [1],
            [ Define to 1 to remove most messages from the kernel driver. ])
fi

AC_ARG_ENABLE([verbose-driver], [AS_HELP_STRING([--enable-verbose-driver],
      [build a very chatty kernel driver (for debugging only)])],
    [if test "x$withval" = "xno"; then
   DRIVER_VERBOSE=no
else
   DRIVER_VERBOSE=yes
fi],
[DRIVER_VERBOSE=no])

if test "x$DRIVER_VERBOSE" = "xyes"; then
  AC_DEFINE(DRIVER_VERBOSE, [1],
            [ Define to 1 to add debugging messages to the kernel driver. ])
fi

AC_ARG_ENABLE([bigphysarea],
            [AS_HELP_STRING([--enable-bigphysarea], [compile the drivers
            with bigphysarea support.])],
            [DRIVER_BIGPHYS=$enableval], [DRIVER_BIGPHYS=no])

if test "x$DRIVER_BIGPHYS" = "xyes"; then
  AC_DEFINE(BIGPHYS, [1],
            [ Define to 1 to use bigphysarea in the kernel driver. ])
fi

AC_ARG_ENABLE([fakemce],
            [AS_HELP_STRING([--enable-fakemce], [build the fakemce device ]
            [into the legacy kernel driver (if built)])],
            [DRIVER_FAKEMCE=$enableval], [DRIVER_FAKEMCE=no])
if test "x$DRIVER_FAKEMCE" = "xyes"; then
  AC_DEFINE(FAKEMCE, [1],
            [ Define to 1 to enable fakemce in the legacy driver. ])
fi
AC_SUBST(DRIVER_FAKEMCE)


dnl Always off, but here so autoheader picks it up.
if false; then
  AC_DEFINE(REALTIME, [1],
            [ Define to 1 to enable realtime in the legacy driver. ])
fi


# Check for php for config2
AC_CHECK_PROG([PHP_IS], [php], [yes], [no])
if test "$PHP_IS" = "no"; then
  AC_MSG_ERROR([cannot find php; perhaps you need to install php5-cli])
fi

# Configure the kernel source directory.

KERNEL_DIR=/lib/modules/`uname -r`
AC_ARG_WITH([kernel-dir],
            [AS_HELP_STRING([--with-kernel-dir=DIR],
                            [set the target kernel source tree to DIR ]
                            [(e.g. $KERNEL_DIR)])],
            [KERNEL_DIR=$withval], [])
AC_SUBST(KERNEL_DIR)


# Allow the user to suppress/relocate swig/python compile/install

AC_ARG_VAR([PYTHON], [Set to the full path to the python interpreter])
AC_ARG_WITH([python],
            [AS_HELP_STRING([--with-python=PATH],
                            [use the python located at PATH])],
                            [PYTHON=$withval], [PYTHON=python])
AC_ARG_ENABLE([python],
               [AS_HELP_STRING([--disable-python],
                               [don't compile the python or swig components])],
                               [DO_MAS_SWIG=${enableval}], [DO_MAS_SWIG=yes])
dnl'
AC_SUBST([DO_MAS_SWIG])

# MAS user and group are configurable

AC_ARG_WITH([user],
            [AS_HELP_STRING([--with-user=LOGIN],[set default MAS user])],
            [MAS_USER=$withval], [MAS_USER=mce])
AC_ARG_WITH([group],
            [AS_HELP_STRING([--with-group=GROUP],[set default MAS group])],
            [MAS_GROUP=$withval], [MAS_GROUP=mce])
AC_SUBST(MAS_USER)
AC_SUBST(MAS_GROUP)
AC_DEFINE_UNQUOTED([MAS_USER], ["$MAS_USER"], [ The MAS user ])
AC_DEFINE_UNQUOTED([MAS_GROUP], ["$MAS_GROUP"], [ The MAS group ])


# MAS install path is configurable via --prefix

MAS_PREFIX="\${prefix}"
AC_SUBST(MAS_PREFIX)


# The location of libconfig is configurable.  libconfigurable.

AC_ARG_WITH([libconfig],
            [AS_HELP_STRING([--with-libconfig=PATH],
              [specify location of libconfig, e.g. /usr/local/])],
            [LIBCONFIG_DIR=$withval],
            [LIBCONFIG_DIR=])
if test "$LIBCONFIG_DIR" != ""; then
  LIBCONFIG_ARGS="-L$LIBCONFIG_DIR/lib"
else
  LIBCONFIG_ARGS=""
fi
LIBCONFIG_ARGS="$LIBCONFIG_ARGS -lconfig"
AC_SUBST(LIBCONFIG_ARGS)


# Check for libconfig.

AC_CHECK_LIB([config], [config_init], ,
             [AC_MSG_FAILURE([could not find libconfig, ]
             [please install libconfig-dev])],
             "$LIBCONFIG_ARGS" )


# Check for pthread library
AC_SEARCH_LIBS([pthread_create],[pthread])


# Check for libreadline

AC_CHECK_HEADER([readline/readline.h], ,
             [AC_MSG_FAILURE([please install the libreadline-dev package.])])

# old versions of readline don't automatically bring in ncurses:
AC_SEARCH_LIBS([readline], [readline], , [
  AC_SEARCH_LIBS([tgetent], [ncurses], [
    # found ncurses, invalidate the cache and try again
    unset ac_cv_search_readline
    AC_SEARCH_LIBS([readline], [readline], ,
      # found ncurses, but still readline doesn't work
      [AC_MSG_FAILURE([please install the libreadline package.])])
    ],[
       # no ncurses, so just complain about readline
       AC_MSG_FAILURE([please install the libreadline package.])])])



# Check for bigphysarea.

check_bigphys=no
if test "$DO_C7_DRIVER" != "no" && test "$DRIVER_BIGPHYS" != "no"; then
  check_bigphys=yes;
elif test "$DO_C6_DRIVER" != "no" && test "$DRIVER_BIGPHYS" != "no"; then
  check_bigphys=yes;
fi

if test "$check_bigphys" = "yes"; then

BIGPHYS_H=${KERNEL_DIR}/build/include/linux/bigphysarea.h
AC_CHECK_HEADER([$BIGPHYS_H],
                [], [AC_MSG_FAILURE(
                 [could not find bigphysarea.h in the kernel build tree])])
fi


dnl Check for swig and python dev headers.

if test "$DO_MAS_SWIG" != "no"; then
  PYTHON_VERSION=`$PYTHON -c "import sys; print sys.version[[:3]]"`
  PYTHON_PREFIX=`$PYTHON -c "import sys; print sys.prefix"`
  PYTHON_EXEC_PREFIX=`$PYTHON -c "import sys; print sys.exec_prefix"`
  PYTHON_LIBDIR=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_config_var('LIBDIR')"`
  PYTHON_SYSLIBS=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_config_var('SYSLIBS')"`
  PYTHON_SHLIBS=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_config_var('SHLIBS')"`
  PYTHON_MODLIBS=`$PYTHON -c "from distutils import sysconfig; print sysconfig.get_config_var('MODLIBS')"`

  PYTHON_CPPFLAGS=-I${PYTHON_PREFIX}/include/python${PYTHON_VERSION}
  if test "x${PYTHON_PREFIX}" != "x${PYTHON_EXEC_PREFIX}"; then
    PYTHON_CPPFLAGS="${PYTHON_CPPFLAGS} -I${PYTHON_EXEC_PREFIX}/include/python${PYTHON_VERSION}"
  fi
  PYTHON_LIBS="-L${PYTHON_LIBDIR} ${PYTHON_SYSLIBS} ${PYTHON_SHLIBS} ${PYTHON_MODLIBS} -lpython${PYTHON_VERSION}"

  AC_SUBST([PYTHON])
  AC_MSG_CHECKING([Python includes])
  AC_MSG_RESULT([$PYTHON_CPPFLAGS])
  AC_MSG_CHECKING([Python libraries])
  AC_MSG_RESULT([$PYTHON_LIBS])

  AC_CHECK_PROG(HAS_SWIG, swig, yes, no)
  if test "$HAS_SWIG" != "yes"; then
    AC_MSG_FAILURE([please install swig package or pass --disable-python.])
  fi
  saved_CPPFLAGS=${CPPFLAGS}
  CPPFLAGS="${CPPFLAGS} ${PYTHON_CPPFLAGS}"
  AC_CHECK_HEADER([Python.h], ,
                   [AC_MSG_FAILURE([could not find Python.h; perhaps you want ]
                   [--with-python=... or --disable-python, or you need to install python-dev])])

  AC_MSG_CHECKING([for NumPy])
  cat > conftest.py << EOF
import sys
try:
  import numpy
except ImportError:
  sys.exit(1)
EOF
  if ! ${PYTHON} conftest.py > /dev/null 2>&1; then
    AC_MSG_FAILURE([could not import Numpy; perhaps you want ]
    [--disable-python, or you need to install python-numpy])
  fi
  AC_MSG_RESULT([yes])
  AC_MSG_CHECKING([NumPy includes])
  NUMPY_CPPFLAGS=-I`$PYTHON -c "import numpy; print numpy.get_include()"`
  AC_MSG_RESULT([$NUMPY_CPPFLAGS])
  PYTHON_CPPFLAGS="${PYTHON_CPPFLAGS} ${NUMPY_CPPFLAGS}"
  CPPFLAGS="${CPPFLAGS} ${NUMPY_CPPFLAGS}"

  AC_CHECK_HEADERS([numpy/arrayobject.h],,
                   [AC_MSG_FAILURE([could not find numpy/arrayobject.h; perhaps you want ]
                   [--disable-python, or you need to install python-numpy])],
                   [
                    #include<Python.h>
                    ])
  CPPFLAGS=$saved_CPPFLAGS
  AC_SUBST([PYTHON_CPPFLAGS])
  AC_SUBST([PYTHON_LIBS])
fi


dnl Paths
AC_ARG_WITH([etc-dir], [AS_HELP_STRING([--with-etc-dir=DIR],
           [the default location of the hardware config files.  (/etc/mce is ]
           [used if not specified.)])], [MAS_ETCDIR="${withval}"],
           [MAS_ETCDIR="no"])
if test "x$MAS_ETCDIR" = "xno"; then
  MAS_ETCDIR="/etc/mce"
fi
AC_SUBST([MAS_ETCDIR])

AC_ARG_WITH([mas-root], [AS_HELP_STRING([--with-mas-root=DIR],
            [the default location of the MCE Script tree. ]
            [("PREFIX/mce_script" is used if not specified.)])],
            [MAS_ROOTDIR="${withval}"], [MAS_ROOTDIR="no"])

AC_ARG_WITH([jam-dir], [AS_HELP_STRING([--with-jam-dir=DIR],
            [the default location for JAM (firmware) files. ]
            [(For multicard MAS, "PREFIX/firmware" is used if not specified. ]
            [For single-card MAS, "MAS_ROOT/firmware" is used if present, ]
            [otherwise "PREFIX/firmware" is used.)])],
            [MAS_JAMDIR="${withval}"], [MAS_JAMDIR="no"])
legacy_jamdir=false
if test "x$MAS_JAMDIR" = "xno" -a $MAX_FIBRE_CARD -eq 1; then
  # look for an existing $MAS_ROOT/firmware
  if test "x$MAS_ROOTDIR" = "xno"; then
    cat <<EOF >conftest.sh

prefix=$prefix
test "x$prefix" = xNONE && prefix=$ac_default_prefix
echo \$prefix/mce_script
EOF
    mas_root=`$SHELL conftest.sh`
  else
    mas_root=$MAS_ROOTDIR
  fi
  if test -d $mas_root/firmware; then
    legacy_jamdir=true
  fi
fi

dnl in all of the following path malarkey the _PHP variables contain embedded
dnl php syntax while the _MAKE variables contain embedded make syntax.  If we
dnl were to use php to generate the makefile, we could get rid of this
dnl duplication.  Then we'd have a m4 -> sh -> php -> make -> CPP -> C chain to
dnl resolve paths.  Fun times for all.
dnl
dnl Note to self: after doing that, remember to replace this hard-coded
dnl configure.ac with a python script that pipes the output of a perl script
dnl through a ruby script to generate this file.

if test "x$MAS_JAMDIR" != "xno"; then
  MAS_JAMDIR_PHP="\"$MAS_JAMDIR\""
  MAS_JAMDIR_MAKE="${MAS_JAMDIR}"
elif $legacy_jamdir; then
  MAS_JAMDIR_PHP="\$mas_root . \"/firmware\""
  MAS_JAMDIR_MAKE="\$(MAS_DATAROOT)/firmware"
else
  MAS_JAMDIR_PHP="\$prefix . \"/firmware\""
  MAS_JAMDIR_MAKE="\$(prefix)/firmware"
fi
AC_SUBST([MAS_JAMDIR_PHP])
AC_SUBST([MAS_JAMDIR_MAKE])

if test "x$MAS_ROOTDIR" = "xno"; then
  MAS_ROOTDIR_PHP="\$prefix . \"/mce_script\""
  MAS_ROOTDIR_MAKE="\$(prefix)/mce_script"
else
  MAS_ROOTDIR_PHP="\"$MAS_ROOTDIR\""
  MAS_ROOTDIR_MAKE="$MAS_ROOTDIR"
fi
AC_SUBST([MAS_ROOTDIR_PHP])
AC_SUBST([MAS_ROOTDIR_MAKE])

AC_ARG_WITH([mas-config], [AS_HELP_STRING([--with-mas-config=DIR],
            [the default MCE Script configuration directory. ]
            [(PREFIX/config is used if not specified.)])],
            [MAS_CONFDIR="${withval}"], [MAS_CONFDIR="no"])
if test "x$MAS_CONFDIR" = "xno"; then
  MAS_CONFDIR_PHP="\$prefix . \"/config\""
  MAS_CONFDIR_MAKE="\$(prefix)/config"
else
  MAS_CONFDIR_PHP="\"$MAS_CONFDIR\""
  MAS_CONFDIR_MAKE="$MAS_CONFDIR"
fi
AC_SUBST([MAS_CONFDIR_PHP])
AC_SUBST([MAS_CONFDIR_MAKE])

AC_ARG_WITH([mas-temp], [AS_HELP_STRING([--with-mas-temp=DIR],
            [the temporary directory for use by MAS and MCE Script. ]
            [(/tmp is used if not specified.)])],
            [MAS_TEMPDIR="${withval}"], [MAS_TEMPDIR="no"])
if test "x$MAS_TEMPDIR" = "xno"; then
  MAS_TEMPDIR="/tmp";
fi
AC_SUBST([MAS_TEMPDIR])

AC_ARG_WITH([data-root], [AS_HELP_STRING([--with-data-root=DIR],
            [the base data directory used by MAS and MCE Script.  For ]
            [multicard MAS, if DIR contains one or more commas (,) it's ] dnl'
            [considered a list, otherwise, an integer fibre card number will ]
            [be appended to DIR for each supported card.  (For multicard MAS, ]
            ["/data/mce" is used if not specified.  For single-card MAS, ]
            ["/data/cryo" is used if not specified for backwards ]
            [compatibility.)])],
            [MAS_DATAROOT="${withval}"], [MAS_DATAROOT="no"])
if test "x$MAS_DATAROOT" = "xno"; then
  if test $MAX_FIBRE_CARD -eq 1; then
    MAS_DATAROOT="/data/cryo"
  else
    MAS_DATAROOT="/data/mce"
  fi
fi
AC_SUBST([MAS_DATAROOT])

AC_ARG_WITH([mas-data], [AS_HELP_STRING([--with-mas-data=SUBDIR],
            [the subdirectory under DATA_ROOT (see --with-data-root above) ]
            [in which data is actually written. ("current_data" is used if not ]
            [specified.)])], [MAS_DATADIR="${withval}"], [MAS_DATADIR="no"])
if test "x$MAS_DATADIR" = "xno"; then
  MAS_DATADIR="current_data";
fi
AC_SUBST([MAS_DATADIR])


dnl Write.

AC_CONFIG_HEADERS([defaults/config.h])
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([applications/Makefile])
AC_CONFIG_FILES([applications/mas_var/Makefile])
AC_CONFIG_FILES([config2/Makefile])
AC_CONFIG_FILES([config2/mas.cin])
AC_CONFIG_FILES([driver_c7/Makefile])
AC_CONFIG_FILES([driver_c6/Makefile])
AC_CONFIG_FILES([include/mce_library.h])
AC_CONFIG_FILES([makefiles/Makefile.install makefiles/Makefile.paths])
AC_CONFIG_FILES([scripts/mas scripts/mas_make_card_id scripts/mas_mknodes],
                [chmod a+x $ac_file])
AC_CONFIG_FILES([scripts/91-mas.rules])
AC_OUTPUT

dnl Handy configure summary

echo
echo "Configure summary:"
echo "  kernel driver (U0107+)   ${DO_C7_DRIVER}"
if test "x$DO_C7_DRIVER" != "xno"; then
  echo "    ... name:              ${DRIVER_NAME}"
  echo "    ... bigphysarea:       ${DRIVER_BIGPHYS}"
  echo "    ... verbose debugging: ${DRIVER_VERBOSE}"
  echo "    ... disable error msg: ${DRIVER_QUIET}"
  echo
fi
echo "  legacy driver (U0106)    ${DO_C6_DRIVER}"
if test "x$DO_C6_DRIVER" != "xno"; then
  echo "    ... name:              ${DRIVER_NAME}"
  echo "    ... bigphysarea:       ${DRIVER_BIGPHYS}"
  echo "    ... fakemce:           ${DRIVER_FAKEMCE}"
  echo "    ... verbose debugging: ${DRIVER_VERBOSE}"
  echo "    ... disable error msg: ${DRIVER_QUIET}"
  echo
fi
echo "  MCE ops:                 ${DO_KERNEL_IO}"
echo "  shared libraries:        ${SHARED}"
if test $MAX_FIBRE_CARD -eq 1; then
  echo "  Multicard support:       no"
else
  echo "  Multicard support:       yes (max ${MAX_FIBRE_CARD} cards)"
fi
echo "  build mce.cfg:           ${DO_CONFIG2}"
if test $INSTALL_SYS_LINKS -eq 1; then
  echo "  Symlinks in /usr/local:  yes"
else
  echo "  Symlinks in /usr/local:  no"
fi
echo "  MAS user/group:          ${MAS_USER}/${MAS_GROUP}"
echo
echo "  etc dir:                 \"${MAS_ETCDIR}\""
echo "  config dir:              ${MAS_CONFDIR_PHP}"
echo "  jam dir:                 ${MAS_JAMDIR_PHP}"
echo "  mce_script dir:          ${MAS_ROOTDIR_PHP}"
echo "  temp dir:                \"${MAS_TEMPDIR}\""
echo "  data root:               \"${MAS_DATAROOT}\""
echo "  data subdir:             \"${MAS_DATADIR}\""

if test "x$DO_CONFIG2" != "xno" -a ! -f config2/$FIRST_MCE_CIN; then
  echo
  if test $MAX_FIBRE_CARD -eq 1; then
    echo "
  Before building MAS, you must first select the mce.cin file to use as your
  hardware configuration file.  To do this, copy an appropriate template file
  from config2/templates to config2/mce.cin.  Then modify this file to
  match the configuration of your subrack.
"
  else
    echo "
  Before building MAS, you must first select the mce.cin files to use as your
  hardware configuration files.  For each fibre card in your system, copy an
  appropriate template files from config2/templates to config2/mce#.cin,
  (where # is the fibre card number).  Then modify each of these files to match
  the configuration of the subrack attached to the given fibre card.
"
  fi
fi
