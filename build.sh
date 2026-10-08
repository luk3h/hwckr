#!/bin/bash

cd "$(dirname "$0")" || exit 1
mkdir -p build && cd build || exit 1
cmake ..
make -j$(nproc)
