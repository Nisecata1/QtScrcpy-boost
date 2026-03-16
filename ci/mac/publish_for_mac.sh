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
    echo "usage: $(basename "$0") <publish_dir> <x64|arm64> <Debug|Release|MinSizeRel|RelWithDebInfo>"
}

if [ -z "$1" ] || [ -z "$2" ] || [ -z "$3" ]; then
    usage
    exit 1
fi

publish_dir="$1"
cpu_arch="$2"
build_mode="$3"

if [[ "$cpu_arch" != "x64" && "$cpu_arch" != "arm64" ]]; then
    echo "error: unknown cpu mode -- $cpu_arch"
    usage
    exit 1
fi

if [[ "$build_mode" != "Release" && "$build_mode" != "Debug" && "$build_mode" != "MinSizeRel" && "$build_mode" != "RelWithDebInfo" ]]; then
    echo "error: unknown build mode -- $build_mode"
    usage
    exit 1
fi

if [ "$cpu_arch" = "x64" ]; then
    qt_clang_path="$ENV_QT_PATH/clang_64"
else
    qt_clang_path="$ENV_QT_PATH/macos"
fi

echo current cpu mode: "$cpu_arch"
echo current build mode: "$build_mode"
echo current publish dir: "$publish_dir"

keymap_path="$script_path/../../keymap"
publish_path="$script_path/$publish_dir"
release_path="$script_path/../../output/$cpu_arch/$build_mode"

if [ ! -d "$release_path" ]; then
    echo "error: build output not found -- $release_path"
    cd "$old_cd"
    exit 1
fi

export PATH="$qt_clang_path/bin:$PATH"

if [ -d "$publish_path" ]; then
    rm -rf "$publish_path"
fi

cp -r "$release_path" "$publish_path"
cp -r "$keymap_path" "$publish_path/QtScrcpy.app/Contents/MacOS"

macdeployqt "$publish_path/QtScrcpy.app"

rm -rf "$publish_path/QtScrcpy.app/Contents/PlugIns/iconengines"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqgif.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqicns.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqico.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqmacheif.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqmacjp2.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqtga.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqtiff.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqwbmp.dylib"
rm -f "$publish_path/QtScrcpy.app/Contents/PlugIns/imageformats/libqwebp.dylib"
rm -rf "$publish_path/QtScrcpy.app/Contents/PlugIns/virtualkeyboard"
rm -rf "$publish_path/QtScrcpy.app/Contents/PlugIns/printsupport"
rm -rf "$publish_path/QtScrcpy.app/Contents/PlugIns/platforminputcontexts"
rm -rf "$publish_path/QtScrcpy.app/Contents/PlugIns/bearer"
rm -rf "$publish_path/QtScrcpy.app/Contents/Frameworks/QtVirtualKeyboard.framework"
rm -rf "$publish_path/Contents/Frameworks/QtSvg.framework"
rm -rf "$publish_path/QtScrcpy.app/Contents/Frameworks/QtQml.framework"
rm -rf "$publish_path/QtScrcpy.app/Contents/Frameworks/QtQuick.framework"

echo
echo
echo ---------------------------------------------------------------
echo finish!!!
echo ---------------------------------------------------------------

cd "$old_cd"
exit 0
