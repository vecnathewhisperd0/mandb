# man-libseccomp.m4 serial 1
dnl MAN_LIBSECCOMP
dnl Check for the libseccomp library.
AC_DEFUN([MAN_LIBSECCOMP],
[PKG_CHECK_MODULES([libseccomp], [libseccomp],
	[AC_DEFINE([HAVE_LIBSECCOMP], [1],
		[Define to 1 if you have the `libseccomp' library.])],
	[:])
]) # MAN_LIBSECCOMP
