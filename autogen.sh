#! /bin/sh -e

gnulib-tool --update >/dev/null
patch -s -p0 < gnulib/lib/xmalloc.patch
autoreconf -i "$@"
