# man-arg-systemdtmpfilesdir.m4 serial 1
dnl MAN_ARG_SYSTEMDTMPFILESDIR
dnl Add a --with-systemdtmpfilesdir option.

AC_DEFUN([MAN_ARG_SYSTEMDTMPFILESDIR],
[
AC_ARG_WITH([systemdtmpfilesdir],
[AS_HELP_STRING([--with-systemdtmpfilesdir=DIR], [Directory for systemd tmpfiles configuration])],
	[], [dnl
	# The default is not prefix-sensitive, since systemd's prefix is not
	# necessarily the same as man-db's.
	with_systemdtmpfilesdir=/usr/lib/tmpfiles.d])
AC_SUBST([systemdtmpfilesdir], [$with_systemdtmpfilesdir])
])
