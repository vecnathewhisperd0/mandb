#! /bin/sh -e

aclocal
autopoint
autoheader && echo timestamp > stamp-h.in
autoconf
