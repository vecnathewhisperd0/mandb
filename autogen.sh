#! /bin/sh -e

autopoint
aclocal-1.10 -I m4
autoheader && echo timestamp > stamp-h.in
autoconf
automake-1.10 --add-missing --copy
