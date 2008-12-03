#! /bin/sh -e

if type gnulib-tool >/dev/null 2>&1; then
	gnulib-tool --update >/dev/null
	if [ -e tools/config.rpath ]; then
		mv tools/config.rpath tools/config.rpath.gnulib
	fi
	patch -s -p0 < gnulib/lib/xmalloc.patch
fi
autoreconf -fi "$@"
if type gnulib-tool >/dev/null 2>&1; then
	if [ -e tools/config.rpath.gnulib ]; then
		mv tools/config.rpath.gnulib tools/config.rpath
	fi
fi
