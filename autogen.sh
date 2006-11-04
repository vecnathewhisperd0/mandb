#! /bin/sh -e

autopoint
aclocal-1.8
autoheader && echo timestamp > stamp-h.in
autoconf
