#!/bin/sh

# Script to create a .tar.bz2 package of the jitify-core source
# Usage:
#   1. Check out the revision of jitify-core that you want to package
#   2. make clean ragel
#   3. package.sh version-number
#      e.g., package.sh 1.0.3

if [ $# -ne 1 ] ;then
  echo "Usage: package.sh version-number"
  exit 1
fi

VERSION=$1
STAGEDIR="jitify-$VERSION"
mkdir -p $STAGEDIR
rm -rf $STAGEDIR/*
FILES="CHANGES INSTALL LICENSE README Makefile src"
find $FILES |cpio -dump $STAGEDIR
tar cf - $STAGEDIR | bzip2 > jitify-core-$VERSION.tar.bz2
