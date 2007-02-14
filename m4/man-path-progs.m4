# man-path-progs.m4 serial 1
dnl MAN_PATH_PROGS(VARIABLE, WITH-ARG, HELP-STRING, PROGS-TO-CHECK-FOR)
dnl Look for a program to use as VARIABLE. If --with-VARIABLE is given, then
dnl set VARIABLE to the path provided there (WITH-ARG and HELP-STRING are
dnl used for --help output). Otherwise, set VARIABLE to the first of
dnl PROGS-TO-CHECK-FOR on the path; if none of those is found, leave
dnl VARIABLE unchanged.
AC_DEFUN([MAN_PATH_PROGS],
[AC_ARG_WITH([$1], [AS_HELP_STRING([--with-$1=$2], [$3])],
             [AS_IF([test "$withval" = yes || test "$withval" = no],
                    [AC_MSG_ERROR([--with-$1 requires an argument])],
                    [AC_MSG_CHECKING([for $1])
                     AC_MSG_RESULT([$withval])
                     $1="$withval"])],
             [AC_PATH_PROGS([$1], [$4])])]) # MAN_PATH_PROGS
