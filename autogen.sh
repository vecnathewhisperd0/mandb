#! /bin/sh
set -e

if type gnulib-tool >/dev/null 2>&1; then
	autopoint -f
	gnulib-tool --update >/dev/null
	patch -s -p1 <gnulib/argp-domain.patch
	patch -s -p1 <gnulib/fdutimens-hurd.patch
	AUTOPOINT=true
	export AUTOPOINT
fi
LIBTOOLIZE_OPTIONS=--quiet
export LIBTOOLIZE_OPTIONS
autoreconf -fi "$@"
