dnl Process this file with autoconf to produce a configure script.
dnl
dnl Initialise and check we're in the correct directory.
dnl
AC_INIT(man-db, 2.5.0)
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR(src/man.c)
AC_CONFIG_AUX_DIR(tools)
AC_CONFIG_HEADER(config.h)
AC_CANONICAL_HOST
AC_ARG_PROGRAM
dnl
dnl Define below date and version information to be put into man pages etc.
dnl
datemark=20071007
date=`LANG=C date -d $datemark +'%Y-%m-%d'`
AC_SUBST(date)dnl
dnl
PACKAGE=AC_PACKAGE_NAME
VERSION=AC_PACKAGE_VERSION
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE",
		   [Define to the name of the distribution.])
AC_DEFINE_UNQUOTED(VERSION, "$VERSION",
		   [Define to the version of the distribution.])
AC_SUBST(PACKAGE)dnl
AC_SUBST(VERSION)dnl
dnl
dnl We have to be a bit naughty here and supply options.
dnl The autoconf literature states that only features that can be 
dnl separately 'built' should use --enable and friends. Oh well...
dnl Current options: setuid, device, db.
dnl 
AC_ARG_ENABLE(setuid,
[AS_HELP_STRING([--enable-setuid[=ARG]], [install man setuid to user ARG [ARG=man]])
AS_HELP_STRING([--disable-setuid], [don't install man setuid])],
	  [if test "$enableval" = "yes" 
	   then
		enableval=man
	   fi
	   if test "$enableval" = "no" 
	   then
		man_owner=
		man_install_flags="-m 755"
		AC_MSG_NOTICE([Man will not be installed setuid])
	   else
		man_owner=$enableval
		man_install_flags="-m 4755 -o $man_owner"
		AC_MSG_NOTICE([Man will be installed setuid $enableval])
	   	AC_DEFINE_UNQUOTED(SECURE_MAN_UID, "$man_owner",
				   [Define as the setuid owner of man or undefine if not installing setuid.])
	   fi],
	  [man_owner=man
	   man_install_flags="-m 4755 -o man"
	   AC_DEFINE_UNQUOTED(SECURE_MAN_UID, "$man_owner")])
AC_ARG_ENABLE(undoc,
[AS_HELP_STRING([--enable-undoc=COMMAND], [suggest COMMAND for missing manual pages])],
	[if test "$enableval" = "yes" || test "$enableval" = "no"
	 then
		AC_MSG_ERROR(--enable-undoc requires an argument)
	 else
		AC_MSG_NOTICE([Suggesting '$enableval' for missing manual pages])
		AC_DEFINE_UNQUOTED(UNDOC_COMMAND, "$enableval",
				   [Define as the name of a command you want to suggest when a non-existent page is requested.])
	 fi])
AC_ARG_ENABLE(mb-groff,
[AS_HELP_STRING([--enable-mb-groff], [expect groff with Debian multibyte patch])],
	[if test "$enableval" = "yes"
	 then
		 AC_MSG_NOTICE([Including support for groff with Debian multibyte patch])
		 AC_DEFINE(MULTIBYTE_GROFF, 1,
			   [Define if your groff installation has the Debian multibyte patch.])
	 fi])
AC_ARG_WITH(device,
[AS_HELP_STRING([--with-device=DEVICE], [use nroff with the output device DEVICE])],
	[if test "$withval" = "yes" || test "$withval" = "no" 
	 then
	 	AC_MSG_ERROR(--with-device requires an argument)
	 else
	 	nroff_device=" -T$withval"
	 fi])
AC_ARG_WITH(db,
[AS_HELP_STRING([--with-db=LIBRARY], [use database library LIBRARY (db4, db3, db2, db1, db, gdbm, ndbm)])],
	[if test "$withval" = "yes" || test "$withval" = "no" 
	 then
	 	AC_MSG_ERROR(--with-db requires an argument)
	 else
	 	db=$withval
	 fi],
	 : ${db=no})
AC_ARG_WITH(config-file,
[AS_HELP_STRING([--with-config-file=CF], [use config file CF [CF=SYSCONFDIR/man_db.conf]])],
	[if test "$withval" = "yes" || test "$withval" = "no"
	 then
		AC_MSG_ERROR(--with-config-file requires an argument)
	 else
		config_file=$withval
		config_file_basename=${withval##*/}
	 fi],
	 : ${config_file=\$\{sysconfdir\}/man_db.conf}
	 : ${config_file_basename=man_db.conf})
dnl
dnl Finish the argument parsing
dnl
AC_SUBST(man_install_flags)dnl
AC_SUBST(config_file)dnl
AC_SUBST(config_file_basename)dnl
dnl
dnl Check $PATH for the following progs and append suitable options.
dnl 
AC_GNU_SOURCE
AC_PROG_CC
AC_PROG_CPP
CFLAGS="$CFLAGS -Wall"
if test "$GCC" = yes
then
	CFLAGS="$CFLAGS -W -Wpointer-arith -Wwrite-strings -Wstrict-prototypes -Wshadow"
fi
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_PROGS(cat, cat)
MAN_CHECK_PROGS([browser], [BROWSER], [use BROWSER as default web browser], [www-browser lynx])
test -n "$browser" && browser="exec $browser"
AC_CHECK_PROGS(tr, tr)
AC_CHECK_PROGS(grep, grep)
MAN_CHECK_PROGS([pager], [PAGER], [use PAGER as default pager], [pager less more])
test -n "$pager" && pager="$pager -s"
dnl
dnl define below (in list of preference) *roff macros to check for.
dnl
macros="andoc an doc"
dnl
dnl We have problems here, as different systems have different *roff
dnl formatters and they accept different options and do different things :(
dnl
MAN_CHECK_PROGS([nroff], [NROFF], [use NROFF as roff formatter for character devices], [nroff gnroff groff])
if test -n "$nroff"
then
	dnl define GNU_NROFF if necessary
	MAN_PROG_GNU_NROFF($nroff)
	if test -n "$nroff_device" 
	then
		AC_MSG_CHECKING(that nroff works with argument$nroff_device)
dnl
dnl  cannot cache this result as it can change between runs of configure.
dnl
		if $nroff $nroff_device </dev/null >/dev/null 2>&1 3>&1
		then 
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
			AC_MSG_ERROR(nroff does not work with argument$nroff_device)
		fi
	fi
	AC_MSG_CHECKING(for appropriate nroff macro)
	AC_CACHE_VAL(man_cv_prog_nroff_macro, [
	
	for macro in $macros
	do
		if $nroff -m$macro $nroff_device </dev/null >/dev/null 2>&1 3>&1
		then
			man_cv_prog_nroff_macro=-m$macro
			break
		fi
	done])
	
	if test -n "$man_cv_prog_nroff_macro"
	then
		nroff="$nroff ${man_cv_prog_nroff_macro}"
dnl		nroff="$nroff ${man_cv_prog_nroff_macro}${nroff_device}"
		AC_MSG_RESULT($man_cv_prog_nroff_macro)
	else
		AC_MSG_RESULT(ambiguous)
		AC_MSG_WARN(please edit include/manconfig.h and add nroff definition)
	fi 
else
	AC_MSG_WARN(Cannot find an nroff-like program, formatting of manual page source will not be supported.)
	nroff="(nroff not installed)"
	AC_DEFINE(NROFF_MISSING, 1, [Define if you don't have nroff.])
fi
dnl It would be nice to use MAN_CHECK_PROGS here, but how do we determine
dnl TROFF_IS_GROFF?
AC_CHECK_PROGS(troff, groff)
if test -n "$troff"
then
	AC_DEFINE(TROFF_IS_GROFF, 1, [Define if you are using groff as troff.])
	TROFF=groff
else
	AC_CHECK_PROGS(troff, troff gtroff)
	TROFF=troff
fi
if test -n "$troff"
then
	AC_DEFINE(HAS_TROFF, 1, [Define if you have troff.])
	AC_MSG_CHECKING(for appropriate $TROFF options)
	AC_CACHE_VAL(man_cv_prog_troff_options, [
dnl	
dnl do a quick test to see if -t works [AIX needs this], groff doesn't as
dnl it indicates pre-process with tbl.
dnl
	test "$TROFF" = "troff" && $troff -t </dev/null >/dev/null 2>&1 3>&1 \
				&& troff_filter="-t "
	for macro in $macros
	do
		if $troff $troff_filter -m$macro </dev/null >/dev/null 2>&1 3>&1
		then
			man_cv_prog_troff_options="${troff_filter}-m${macro}"
			break
		fi
	done])
	if test -n "$man_cv_prog_troff_options"
	then
		troff="$troff $man_cv_prog_troff_options"
		AC_MSG_RESULT($man_cv_prog_troff_options)
	else
		AC_MSG_RESULT(ambiguous)
		AC_MSG_WARN(please edit include/manconfig.h and add troff definition)
	fi
else
	troff="(troff not installed)"
fi
dnl 
MAN_CHECK_PROGS([eqn], [EQN], [use EQN to preprocess equations], [eqn geqn])
MAN_CHECK_PROGS([neqn], [NEQN], [use NEQN to preprocess equations for character devices], [neqn gneqn])
dnl
dnl if we fail to find an neqn, use eqn and try to force it to output
dnl for an ascii device. As this is only relevant for equations (?), not
dnl using latin1 should be acceptable. -Tlatin1 is ignored by some eqn.
dnl
if test -z "$neqn"
then
	test -n "$eqn" && 
	(test -n "$nroff_device" && neqn="$eqn -T$nroff_device" || neqn="$eqn -Tascii")
fi
MAN_CHECK_PROGS([tbl], [TBL], [use TBL to preprocess tables], [tbl gtbl])
MAN_CHECK_PROGS([col], [COL], [use COL to filter formatting characters from output], [col gcol])
MAN_CHECK_PROGS([vgrind], [VGRIND], [use VGRIND to preprocess program sources], [vgrind gvgrind])
MAN_CHECK_PROGS([refer], [REFER], [use REFER to preprocess bibliographic references], [refer grefer])
MAN_CHECK_PROGS([grap], [GRAP], [use GRAP to preprocess graphs], [grap])
MAN_CHECK_PROGS([pic], [PIC], [use PIC to preprocess pictures], [pic gpic])
test -n "$pic" && pic="$pic -S"
MAN_CHECK_PROGS([gzip], [GZIP], [use GZIP as GNU compression utility], [gzip])
if test -n "$gzip"
then
	gunzip="$gzip -dc"
	compressor="$gzip -c7"
	compress_ext="gz"
fi
MAN_CHECK_PROGS([compress], [COMPRESS], [use COMPRESS as UNIX compression utility], [compress])
if test -n "$compress"
then
	uncompress="$compress -dc"
	if test -z "$gzip"
	then
		compressor="$compress -c"
		compress_ext="Z"
	fi
fi
MAN_CHECK_PROGS([bzip2], [BZIP2], [use BZIP2 as block-sorting compression utility], [bzip2])
if test -n "$bzip2"
then
	bunzip2="$bzip2 -dc"
fi
if test -n "$gzip" || test -n "$compress" || test -n "$bzip2"
then
	AC_DEFINE(COMP_CAT, 1, [Define if you have compressors and want to support compressed cat files.])
	AC_DEFINE(COMP_SRC, 1, [Define if you have compressors and want to support compressed manual source.])
fi
AC_SUBST(compressor)dnl
AC_SUBST(compress_ext)dnl
AC_SUBST(gunzip)dnl
AC_SUBST(uncompress)dnl
AC_SUBST(bunzip2)dnl
AC_CHECK_LIB(z, gzopen)
dnl
dnl to add more decompressors just follow the scheme above.
dnl
dnl Work out which manual page hierarchy scheme might be in use.
dnl
AC_ARG_ENABLE(mandirs,
[AS_HELP_STRING([--enable-mandirs=OS], [select manual page hierarchy organization (GNU, HPUX, IRIX, Solaris, BSD)])],
	[AC_MSG_NOTICE([Using hierarchy organization(s) $enableval])
	 AC_DEFINE_UNQUOTED(MANDIR_LAYOUT, "$enableval",
			    [Define to the manual page hierarchy organization(s) in use.])],
	[case $host in
		*-gnu)          mandirs=GNU;;
		*-hpux*)        mandirs=HPUX;;
		*-irix*)        mandirs=IRIX;;
		*-solaris*)     mandirs=Solaris;;
		*-bsd*)         mandirs=BSD;;
		*)              mandirs=;;
	 esac
	 if test -n "$mandirs"; then
		AC_MSG_NOTICE([Using hierarchy organization $mandirs])
		AC_DEFINE_UNQUOTED(MANDIR_LAYOUT, "$mandirs")
	 else
		AC_MSG_NOTICE([Allowing any hierarchy organization])
		AC_DEFINE(MANDIR_LAYOUT, "")
	 fi])
dnl
dnl check for various header files and associated libraries.
dnl
AC_AIX
AC_MINIX
AC_ISC_POSIX
dnl AC_PROG_LEX calls AC_TRY_LINK: must come after above 3
AC_PROG_LEX
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT
AC_CHECK_HEADERS(unistd.h memory.h fcntl.h limits.h sys/file.h regex.h libgen.h)
AC_CHECK_HEADERS(string.h strings.h, break)
dnl
dnl Internationalization support.
dnl
AM_GNU_GETTEXT
AM_GNU_GETTEXT_VERSION([0.16.1])
AC_SUBST(LINGUAS)dnl
AM_ICONV
dnl
dnl Checks for structures and compiler characteristics.
dnl
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_TYPE_SIZE_T
dnl
dnl Checks for functions.
dnl
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_TYPE_SIGNAL
AC_FUNC_STRCOLL
AC_FUNC_VFORK
AC_FUNC_VPRINTF
MAN_FUNC_ANSI_SPRINTF
MAN_FUNC_PCLOSE
MAN_FUNC_STRSIGNAL
dnl
dnl Find a suitable database interface header and library
dnl
dnl Check for Berkeley db routines (first version API)
dnl
if test "$db" = "no" || test "$db" = "db4" || test "$db" = "db"; then
MAN_CHECK_BDB([db4/db_185.h db_185.h], [db4 db-4],
              [AC_DEFINE_UNQUOTED([BDB_H], [<$head>],
				  [Define if you have, and want to use, Berkeley database header files.])
               AC_DEFINE_UNQUOTED([BTREE], [$lib],
				  [Define if you have, and want to use, the Berkeley database library.])
              ])
fi
dnl
if test "$db" = "no" || test "$db" = "db3" || test "$db" = "db"; then
MAN_CHECK_BDB([db3/db_185.h db_185.h], [db3],
              [AC_DEFINE_UNQUOTED([BDB_H], [<$head>],
				  [Define if you have, and want to use, Berkeley database header files.])
               AC_DEFINE_UNQUOTED([BTREE], [$lib],
				  [Define if you have, and want to use, the Berkeley database library.])
              ])
fi
dnl
if test "$db" = "no" || test "$db" = "db2" || test "$db" = "db"; then
MAN_CHECK_BDB([db_185.h db2/db_185.h db2_185.h], [db2 db],
              [AC_DEFINE_UNQUOTED([BDB_H], [<$head>])
               AC_DEFINE_UNQUOTED([BTREE], [$lib])
              ])
fi
dnl
if test "$db" = "no" || test "$db" = "db1" || test "$db" = "db"; then
MAN_CHECK_BDB([db/db.h db.h db1/db.h], [db db1],
              [AC_DEFINE_UNQUOTED([BDB_H], [<$head>])
               AC_DEFINE_UNQUOTED([BTREE], [$lib])
              ])
fi
dnl
  			       
dnl if test "$db" = "no" || test "$db" = "db2"; then
dnl MAN_CHECK_BDB([db2_185.h], [db2 db c],
dnl               [AC_DEFINE(BTREE, 2)
dnl                AC_DEFINE(DB_ON_LIBC)
dnl               ])
dnl fi
dnl if test "$db" = "no" || test "$db" = "db2"; then
dnl MAN_CHECK_BDB([db2/db_185.h], [db2 db c], [AC_DEFINE(BTREE, 2)])
dnl fi
dnl dnl
dnl dnl from package libdb1-dev
dnl dnl
dnl if test "$db" = "no" || test "$db" = "db"; then
dnl MAN_CHECK_BDB([db/db.h], [db c], [AC_DEFINE(BTREE, 1)])
dnl fi
dnl dnl
dnl dnl from package libc6-dev
dnl dnl
dnl dnl ########## statically linked #####
dnl if test "$db" = "no" || test "$db" = "db"; then
dnl MAN_CHECK_BDB([db1/db.h], [db c],
dnl               [AC_DEFINE(BTREE, 1)
dnl                AC_DEFINE(DB_ON_LIBC)
dnl               ])
dnl fi
dnl
dnl Check for GNU dbm routines
dnl
if test "$db" = "no" || test "$db" = "gdbm"
then
  AC_CHECK_HEADER(gdbm.h,[
    for lib in gdbm c dbm
    do
      AC_CHECK_LIB($lib, gdbm_fetch,
		   test "$lib" = "c" || DBLIBS="-l$lib" 
		   [AC_DEFINE(GDBM, 1, [Define if you have, and want to use, gdbm interface routines.])]
		   db=yes, db=no)
      if test "$db" = "yes" 
      then
dnl Need to explicitly check for some routines, rather than using 
dnl version info extracted from gdbm.h
        AC_CHECK_FUNCS(gdbm_exists gdbm_setopt)
        break
      fi
    done], db=no)
fi
dnl
dnl Check for UNIX ndbm routines
dnl
if test "$db" = "no" || test "$db" = "ndbm"
then
  AC_CHECK_HEADER(ndbm.h,[
    for lib in ndbm c dbm
    do
      AC_CHECK_LIB($lib, dbm_fetch,
		   test "$lib" = "c" || DBLIBS="-l$lib"
		   [AC_DEFINE(NDBM, 1, [Define if you have, and want to use, ndbm interface routines.])]
		   db=yes, db=no)
      test "$db" = "yes" && break
      done], db=no)
fi 
if test "$db" != "yes"
then 
    if test "$db" = "no"
    then
      AC_MSG_ERROR(Fatal: no supported database library/header found)
    else
      AC_MSG_ERROR(Fatal: $db: unsupported database library)
    fi
fi
AC_SUBST(DBLIBS)
dnl
dnl Check for available functions.
dnl
AC_CHECK_FUNCS(getcwd getwd, break)
AC_CHECK_FUNCS(atexit on_exit, break)
AC_CHECK_FUNCS(seteuid setreuid setresuid strerror)
AC_CHECK_FUNCS(canonicalize_file_name)
AC_CHECK_FUNCS(setlocale)
dnl
dnl I think it's reasonable to assume that the system has the following 
dnl headers if the (POSIXy) functions exist.
dnl 
AC_CHECK_FUNC(regcomp, [AC_DEFINE(POSIX_REGEX, 1, [Define if you have the regcomp and regexec POSIX regex routines.])],
  [AC_CHECK_FUNC(re_comp, [AC_DEFINE(BSD_REGEX, 1, [Define if you have the re_comp and re_exec BSD regex routines.])],
    [AC_MSG_WARN(no regex routines found, whatis/apropos will ignore -r)])])
MAN_FUNC_FNMATCH
AC_CHECK_FUNC(globfree,
	      [AC_DEFINE(HAVE_GLOB_H, 1, [Define if using <glob.h> rather than "lib/glob.h".])],
	      [AC_LIBOBJ([glob])])
AC_CHECK_FUNC(getopt_long,
	      [AC_DEFINE(HAVE_GETOPT_H, 1, [Define if using <getopt.h> rather than "lib/getopt.h".])],
	      [AC_LIBOBJ([getopt])
	       AC_LIBOBJ([getopt1])])
AC_REPLACE_FUNCS(flock putenv realpath rename basename waitpid strstr strsep strcspn strspn strnlen mkdtemp mkstemp)
gt_FUNC_SETENV
gl_FUNC_GEN_TEMPNAME
dnl
dnl MAN_ECHO_VAR(ENV-VARIABLE)
define(MAN_ECHO_VAR, [AC_MSG_NOTICE([default $1 = "$$1"])])dnl
dnl
MAN_ECHO_VAR(CC)
MAN_ECHO_VAR(CPP)
MAN_ECHO_VAR(CPPFLAGS)
MAN_ECHO_VAR(CFLAGS)
MAN_ECHO_VAR(LDFLAGS)
MAN_ECHO_VAR(LIBS)
MAN_ECHO_VAR(DBLIBS)

AC_CONFIG_FILES(GNUmakefile
	makefile
	lib/Makefile
	src/Makefile
	src/man_db.conf
	man/Makefile
	manual/Makefile
	libdb/Makefile
	include/Defines
	include/comp_src.h
	include/manconfig.h
	intl/Makefile
	po/Makefile.in)
AC_OUTPUT

date > stamp-h
