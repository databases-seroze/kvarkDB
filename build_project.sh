#!/bin/bash
rm -rf build  && mkdir build
cmake -S . -B build
cmake --build build -v
