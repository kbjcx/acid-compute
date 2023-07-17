#!/bin/bash
sudo pwd
mkdir -p test/http/build

cd build || return
cmake ..
make
