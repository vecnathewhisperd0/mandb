#! /bin/sh -e

# Build automatically generated files
./autogen.sh

# Basic configure to get 'make dist'
./configure

make dist
