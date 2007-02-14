# man-fnmatch.m4 serial 1
dnl Check that fnmatch() supports the GNU extension FNM_CASEFOLD. If
dnl cross-compiling, always use our version.
AC_DEFUN([MAN_FUNC_FNMATCH],
[
AC_CACHE_CHECK([for fnmatch with FNM_CASEFOLD], [man_cv_func_fnmatch],
[AC_RUN_IFELSE(
	[AC_LANG_PROGRAM(
		[[
#			define _GNU_SOURCE
#			include <stdlib.h>
#			include <fnmatch.h>
		]],
		[[exit (fnmatch ("ABcd", "AbCd", FNM_CASEFOLD) != 0);]]
	)],
	[man_cv_func_fnmatch=yes],
	[man_cv_func_fnmatch=no],
	[man_cv_func_fnmatch=no])])
if test "$man_cv_func_fnmatch" = "yes"
then
	AC_DEFINE([HAVE_FNMATCH_H], 1, [Define if using <fnmatch.h> rather than "lib/fnmatch.h".])
fi
if test "$man_cv_func_fnmatch" = "no"
then
	AC_LIBOBJ([fnmatch])
fi
])
