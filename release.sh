#! /bin/sh -e

# Build automatically generated files
./autogen.sh --copy

# Basic configure to get 'make dist'
./configure

make dist
