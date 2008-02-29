#! /bin/sh -e

if type gnulib-tool >/dev/null 2>&1; then
	if [ -e tools/config.rpath ]; then
		mv tools/config.rpath tools/config.rpath.gettext
	fi
	gnulib-tool --update >/dev/null
	if [ -e tools/config.rpath ] && [ -e tools/config.rpath.gettext ]; then
		mv tools/config.rpath tools/config.rpath.gnulib
		mv tools/config.rpath.gettext tools/config.rpath
	fi
	patch -s -p0 < gnulib/lib/xmalloc.patch
fi
autoreconf -fi "$@"
