#! /bin/sh -e

autopoint
aclocal-1.10
autoheader && echo timestamp > stamp-h.in
autoconf
