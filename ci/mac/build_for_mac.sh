#!/bin/bash

echo
echo
echo ---------------------------------------------------------------
echo check ENV
echo ---------------------------------------------------------------
echo ENV_QT_PATH "$ENV_QT_PATH"

script_path=$(cd "$(dirname "$0")" && pwd)
old_cd=$(pwd)
cd "$script_path"

usage() {
    echo "usage: $(basename "$0") <Debug|Release|MinSizeRel|RelWithDebInfo> <x64|arm64>"
}

if [ -z "$1" ] || [ -z "$2" ]; then
    usage
    exit 1
fi

build_mode="$1"
cpu_arch="$2"

if [[ "$build_mode" != "Release" && "$build_mode" != "Debug" && "$build_mode" != "MinSizeRel" && "$build_mode" != "RelWithDebInfo" ]]; then
    echo "error: unknown build mode -- $build_mode"
    usage
    exit 1
fi

if [[ "$cpu_arch" != "x64" && "$cpu_arch" != "arm64" ]]; then
    echo "error: unknown cpu mode -- $cpu_arch"
    usage
    exit 1
fi

echo
echo
echo ---------------------------------------------------------------
echo begin cmake build
echo ---------------------------------------------------------------
echo current build mode: "$build_mode"
echo current cpu mode: "$cpu_arch"

cmake_arch=x86_64
if [ "$cpu_arch" = "x64" ]; then
    qt_cmake_path="$ENV_QT_PATH/clang_64/lib/cmake/Qt5"
else
    qt_cmake_path="$ENV_QT_PATH/macos/lib/cmake/Qt6"
    cmake_arch=arm64
fi

echo qt cmake path: "$qt_cmake_path"

output_path="$script_path/../../output/$cpu_arch/$build_mode"
if [ -d "$output_path" ]; then
    rm -rf "$output_path"
fi

build_path="$script_path/../build_temp"
if [ -d "$build_path" ]; then
    rm -rf "$build_path"
fi
mkdir -p "$build_path"
cd "$build_path"

cmake_params="-DCMAKE_PREFIX_PATH=$qt_cmake_path -DCMAKE_BUILD_TYPE=$build_mode -DCMAKE_OSX_ARCHITECTURES=$cmake_arch"
cmake $cmake_params ../..
if [ $? -ne 0 ] ; then
    echo "cmake failed"
    cd "$old_cd"
    exit 1
fi

cmake --build . --config "$build_mode" -j8
if [ $? -ne 0 ] ; then
    echo "cmake build failed"
    cd "$old_cd"
    exit 1
fi

echo
echo
echo ---------------------------------------------------------------
echo finish!!!
echo ---------------------------------------------------------------

cd "$old_cd"
exit 0
