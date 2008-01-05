# man-gcc-warning.m4 serial 1
dnl MAN_GCC_WARNING(WARNING)
dnl Add -WWARNING to CFLAGS if it is supported by the compiler.
AC_DEFUN([MAN_GCC_WARNING],
[man_saved_CFLAGS="$CFLAGS"
 CFLAGS="$CFLAGS -W$1"
 AC_CACHE_CHECK([that GCC supports -W$1],
   [AS_TR_SH([man_cv_gcc_warning_$1])],
   [AS_TR_SH([man_cv_gcc_warning_$1])=no
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()],
                      [AS_TR_SH([man_cv_gcc_warning_$1])=yes])])
 if test "$AS_TR_SH([man_cv_gcc_warning_$1])" = no; then
   CFLAGS="$man_saved_CFLAGS"
 fi]) # MAN_GCC_WARNING
