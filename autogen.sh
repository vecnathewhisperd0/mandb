#! /bin/sh -e

copy="$1"

aclocal
gettextize $copy --force
autoheader && echo timestamp > stamp-h.in
autoconf
