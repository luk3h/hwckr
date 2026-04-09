#!/bin/bash

cd /home/lukeh/Documents/hwckr/build || exit 1
cmake ..
make -j$(nproc) && ./hwckr
