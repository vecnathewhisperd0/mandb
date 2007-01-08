dnl
dnl Check to see if sprintf() returns the number of chars converted
dnl Take no arguments
dnl
AC_DEFUN([MAN_FUNC_ANSI_SPRINTF],
[AC_MSG_CHECKING([for ANSI C sprintf])

AC_CACHE_VAL([man_cv_func_ansi_sprintf],
[AC_TRY_RUN(
  [int main(void) {
     char str[12];
     if ((int)sprintf(str,"test_string")==11) exit(0);
     exit(1);
   }],
  [man_cv_func_ansi_sprintf=yes],
  [man_cv_func_ansi_sprintf=no],
  [man_cv_func_ansi_sprintf=no])
])

if test "$man_cv_func_ansi_sprintf" = "yes"
then
	AC_DEFINE([ANSI_SPRINTF], 1, [Define if your sprintf function returns a count of converted characters.])
fi

AC_MSG_RESULT([$man_cv_func_ansi_sprintf])
])

dnl
dnl Check to see if nroff is GNU nroff, take nroff path as arg.
dnl
AC_DEFUN([MAN_PROG_GNU_NROFF],
[AC_MSG_CHECKING([whether nroff is GNU nroff])

AC_CACHE_VAL([man_cv_prog_gnu_nroff],
  [if test `$1 <<EOF | tr -d '\n'
\\n(.g
EOF
` -eq 1
   then
	man_cv_prog_gnu_nroff=yes
   else
	man_cv_prog_gnu_nroff=no
   fi])

if test "$man_cv_prog_gnu_nroff" = "yes" 
then
	AC_DEFINE([GNU_NROFF], 1, [Define if nroff is GNU nroff.])
fi
AC_MSG_RESULT([$man_cv_prog_gnu_nroff])
])

dnl
dnl Check to see if popen/pclose correctly handles multiple streams
dnl
AC_DEFUN([MAN_FUNC_PCLOSE],
[AC_MSG_CHECKING([for working pclose])

AC_CACHE_VAL([man_cv_func_pclose],
[AC_TRY_RUN(
[#include <stdio.h>
#include <sys/wait.h>
int
try_pclose (FILE *f, int expected)
{
  int s;
  s = pclose (f);
  if (s != -1 && WIFEXITED (s))
    return WEXITSTATUS (s) != expected;
  else 
    return 1;
}
int
main (void)
{
  FILE *f, *t;
  int i, sf, st;
  for (i = 0;  i <= 1;  ++i) {
    f = popen ("false", "r");
    sleep (1);
    t = popen ("true", "r");
    sleep (1);
    if (i) {
      st = try_pclose (t, 0);
      sf = try_pclose (f, 1);
    } else {
      sf = try_pclose (f, 1);
      st = try_pclose (t, 0);
    }
    if (sf || st) exit(1);
  }
  exit(0);
}],
[man_cv_func_pclose=yes],
[man_cv_func_pclose=no],
[man_cv_func_pclose=no])])

test "$man_cv_func_pclose" = "no" && AC_LIBOBJ(popen)

AC_MSG_RESULT([$man_cv_func_pclose])
])

dnl MAN_CHECK_BDB(HEADERS, LIBS, ACTION-IF-FOUND)
dnl Helper to check Berkeley DB linkage when particular header files and
dnl libraries are included. ACTION-IF-FOUND may contain $head and $lib.
dnl Contributed by Duncan Simpson <dps@io.stargate.co.uk> and hacked into a
dnl macro by Colin Watson.

AC_DEFUN([MAN_CHECK_BDB], [dnl
for head in $1
do
  AC_CHECK_HEADERS([$head], [got=yes], [got=no])
  if test "$got" = "yes"
  then
    for lib in $2
    do
      AS_VAR_PUSHDEF([man_tr_bdb], [man_cv_bdb_header_${head}_lib_${lib}])dnl
      man_saved_LIBS="$LIBS"
      LIBS="$LIBS -l$lib"
      AC_CACHE_CHECK([for dbopen from <${head}> in -l${lib}], man_tr_bdb,
         [AC_TRY_LINK([#include <$head>], [dbopen("foo", 0, 0, 0, (void *) 0)],
                      [AS_VAR_SET(man_tr_bdb, yes)],
                      [AS_VAR_SET(man_tr_bdb, no)])
         ])
      AS_IF([test AS_VAR_GET(man_tr_bdb) = yes],
            [$3
             DBLIBS="-l$lib"
             db=yes],
            [db=no])
      LIBS="$man_saved_LIBS"
      AS_VAR_POPDEF([man_tr_bdb])dnl
      test "$db" = "yes" && break
    done
  fi
  test "$db" = "yes" && break
done[]dnl
])# MAN_CHECK_BDB

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
