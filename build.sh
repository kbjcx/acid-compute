#!/bin/bash
pwd
cd build || return
rm -f CMakeCache.txt
cmake .. -DBUILD_DYNAMIC=ON
for i in "$@"; do
    make $i
done
