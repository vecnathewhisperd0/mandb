#! /bin/sh -e

if type gnulib-tool >/dev/null 2>&1; then
	autopoint -f
	gnulib-tool --update >/dev/null
	patch -s -p0 < gnulib/lib/xmalloc.patch
	export AUTOPOINT=true
fi
autoreconf -fi "$@"
