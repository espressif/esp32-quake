#!/bin/bash

if [ ! -d cmake-build ]
then
	mkdir cmake-build
fi

cd cmake-build

if $1
then
	cmake -D CMAKE_TOOLCHAIN_FILE=$1 ..
else
	cmake ..
fi

make
cd ..
