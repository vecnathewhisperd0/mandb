# man-pclose.m4 serial 1
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
