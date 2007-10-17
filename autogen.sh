#! /bin/sh -e

gnulib-tool --update >/dev/null
autoreconf -i "$@"
