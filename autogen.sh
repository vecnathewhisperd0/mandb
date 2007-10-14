#! /bin/sh -e

autoreconf -i "$@"
gnulib-tool --update >/dev/null
