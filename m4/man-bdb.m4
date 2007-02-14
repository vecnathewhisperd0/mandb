# man-bdb.m4 serial 1
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
