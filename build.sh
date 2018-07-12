#!/usr/bin/env bash

# Pull latest git
git pull --recurse-submodules
git submodule update --init --recursive

# Build Dependency(GLSLang) first
BUILD_DIRECTORY=./glslang/build
if [ -d "$BUILD_DIRECTORY" ]; then
  rm -rf "$BUILD_DIRECTORY"
fi
mkdir $BUILD_DIRECTORY
pushd $BUILD_DIRECTORY
cmake -DCMAKE_CXX_FLAGS=-std=c++11 -DCMAKE_BUILD_TYPE=Release ../glslang
make
popd

# Build Shadertune
make

echo "Finished build Shadertune"