#!/bin/bash

if [ ! -d meson-build ]
then
	mkdir meson-build
fi

if $1
then
	meson setup --cross-file $1 meson-build
else
	meson setup meson-build
fi

cd meson-build
meson compile
cd ..
