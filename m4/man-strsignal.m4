# man-strsignal.m4 serial 1
dnl Check for strsignal(). If not available, check for sys_siglist.
AC_DEFUN([MAN_FUNC_STRSIGNAL],
[
AC_CACHE_CHECK([for strsignal], [man_cv_func_strsignal],
[AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[
#			define _GNU_SOURCE
#			include <string.h>
		]],
		[[char *p = strsignal (1);]]
	)],
	[man_cv_func_strsignal=yes],
	[man_cv_func_strsignal=no])])
if test "$man_cv_func_strsignal" = "yes"
then
	AC_DEFINE([HAVE_STRSIGNAL], 1, [Define to 1 if you have the `strsignal' function.])
fi
if test "$man_cv_func_strsignal" = "no"
then
	AC_DECL_SYS_SIGLIST
fi
])
