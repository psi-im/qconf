#!/bin/sh
set -e

if [ $# -lt 1 ]; then
	echo "usage: $0 [version]"
	exit 1
fi

VERSION=$1

mkdir -p build/qconf-$VERSION
cp -a AUTHORS COPYING README.md TODO conf configure configure.exe examples modules qconf.* src build/qconf-$VERSION
cd build
tar Jcvf qconf-$VERSION.tar.xz qconf-$VERSION

echo; echo Packed $(pwd)/qconf-$VERSION.tar.xz
