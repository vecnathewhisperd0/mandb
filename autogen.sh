#! /bin/sh -e

autopoint
aclocal
autoheader && echo timestamp > stamp-h.in
autoconf
