#! /bin/sh -e

aclocal
autoheader && echo timestamp > stamp-h.in
autoconf
