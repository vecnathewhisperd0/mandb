#! /bin/sh -e

aclocal
autoheader && date > stamp-h.in
autoconf
