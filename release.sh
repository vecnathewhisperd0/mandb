#! /bin/sh -e

# Build automatically generated files
./autogen.sh

# Basic configure to get 'make distcheck'
./configure

# Force regeneration with new version number
rm -f man/po4a/po/man-db-manpages.pot

make distcheck
