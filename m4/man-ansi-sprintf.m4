# man-ansi-sprintf.m4 serial 1
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
