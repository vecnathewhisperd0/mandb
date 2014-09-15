# man-arg-setuid.m4 serial 1
dnl MAN_ARG_SETUID
dnl Add an --enable-setuid option.

AC_DEFUN([MAN_ARG_SETUID],
[
AC_ARG_ENABLE([setuid],
[AS_HELP_STRING([--enable-setuid[=ARG]], [install man setuid to user ARG [ARG=man]])
AS_HELP_STRING([--disable-setuid], [don't install man setuid])],
	  [if test "$enableval" = "yes" 
	   then
		enableval=man
	   fi
	   if test "$enableval" = "no" 
	   then
		man_owner=
		man_mode="755"
		AC_MSG_NOTICE([Man will not be installed setuid])
	   else
		man_owner=$enableval
		man_mode="4755"
		AC_MSG_NOTICE([Man will be installed setuid $enableval])
	   	AC_DEFINE_UNQUOTED([SECURE_MAN_UID], ["$man_owner"],
				   [Define as the setuid owner of man or undefine if not installing setuid.])
	   fi],
	  [man_owner=man
	   man_mode="4755"
	   AC_DEFINE_UNQUOTED([SECURE_MAN_UID], ["$man_owner"])])
AC_SUBST([man_owner])
AC_SUBST([man_mode])
])
