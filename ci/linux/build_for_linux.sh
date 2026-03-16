#!/bin/bash

echo ---------------------------------------------------------------
echo Check \& Set Environment Variables
echo ---------------------------------------------------------------

echo Current ENV_QT_PATH: $ENV_QT_PATH
echo Current directory: $(pwd)
qt_cmake_path=$ENV_QT_PATH/gcc_64/lib/cmake/Qt5
qt_gcc_path=$ENV_QT_PATH/gcc_64
export PATH=$qt_gcc_path/bin:$PATH

old_cd=$(pwd)
cd "$(dirname "$0")/../../"

usage() {
    echo "usage: $(basename "$0") <Debug|Release|MinSizeRel|RelWithDebInfo>"
}

echo
echo
echo ---------------------------------------------------------------
echo Check Build Parameters
echo ---------------------------------------------------------------
echo Possible build modes: Debug/Release/MinSizeRel/RelWithDebInfo

if [ -z "$1" ]; then
    usage
    exit 1
fi

build_mode="$1"
if [[ "$build_mode" != "Release" && "$build_mode" != "Debug" && "$build_mode" != "MinSizeRel" && "$build_mode" != "RelWithDebInfo" ]]; then
    echo "error: unknown build mode -- $build_mode"
    usage
    exit 1
fi

echo Current build mode: $build_mode

echo
echo
echo ---------------------------------------------------------------
echo CMake Build Begins
echo ---------------------------------------------------------------

output_path="./output/x64/$build_mode"
if [ -d "$output_path" ]; then
    rm -rf "$output_path"
fi

cmake_params="-DCMAKE_PREFIX_PATH=$qt_cmake_path -DCMAKE_BUILD_TYPE=$build_mode"
cmake $cmake_params .
if [ $? -ne 0 ] ;then
    echo "error: CMake failed, exiting......"
    cd "$old_cd"
    exit 1
fi

cmake --build . --config "$build_mode" -j8
if [ $? -ne 0 ] ;then
    echo "error: CMake build failed, exiting......"
    cd "$old_cd"
    exit 1
fi

echo
echo
echo ---------------------------------------------------------------
echo CMake Build Succeeded
echo ---------------------------------------------------------------

cd "$old_cd"
exit 0
