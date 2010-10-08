# MAS_DEFAULT_VAR(VARNAME, DEFAULT, [MULTICARD_DEFAULT])
# ---------------------------------
# Register MASDEFAULT_<VARNAME> as a precious variable, and use <DEFAULT> (or
# <MULTICARD_DEFAULT> if given and multicard is enabled) as it's value if not
# set in the environment nor on the command line.  Then add it to the config
# header.
AC_DEFUN([MAS_DEFAULT_VAR],
[AC_ARG_VAR([MASDEFAULT_$1], [the default (template) for MAS_$1 ]dnl
[("$2" is used if not specified]dnl
ifelse(`x$3', `x',, [[, or "$3", in the case of Multicard MAS]])[)])

if test -z "$MASDEFAULT_$1"; then
ifelse(`x$3', `x',[MASDEFAULT_$1="patsubst($2,[\$],[\\\$])"],
[if test $MAX_FIBRE_CARD -eq 1; then
    MASDEFAULT_$1="patsubst($2,[\$],[\\\$])"
  else
    MASDEFAULT_$1="patsubst($3,[\$],[\\\$])"
  fi])
fi
AC_DEFINE_UNQUOTED([MASDEFAULT_$1], ["$MASDEFAULT_$1"],
				   [ Define to the default for MAS_$1. ])])

