#!/bin/bash

echo "Package AppImage"

usage() {
    echo "usage: $(basename "$0") <Debug|Release|MinSizeRel|RelWithDebInfo>"
}

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

# Qt path detection
detected_qt_path=""
if command -v qmake &> /dev/null; then
    qmake_path=$(which qmake)
    if [ -n "$qmake_path" ]; then
        qt_base=$(dirname "$(dirname "$(dirname "$qmake_path")")")
        if [ -d "$qt_base/gcc_64" ]; then
            detected_qt_path="$qt_base"
        fi
    fi
fi

if [ -n "$detected_qt_path" ]; then
    ENV_QT_PATH="$detected_qt_path"
elif [ -n "$ENV_QT_PATH" ]; then
    if [ ! -d "$ENV_QT_PATH/gcc_64" ]; then
        detected_qt_path=""
    fi
fi

if [ -z "$ENV_QT_PATH" ] || [ ! -d "$ENV_QT_PATH/gcc_64" ]; then
    common_qt_paths=(
        "$HOME/Qt"
        "/opt/Qt"
        "/usr/local/Qt"
        "/usr/lib/qt5"
    )
    for base_path in "${common_qt_paths[@]}"; do
        if [ -d "$base_path" ]; then
            latest_version=$(ls -1td "$base_path"/*/gcc_64 2>/dev/null | head -n 1 | sed 's|/gcc_64$||')
            if [ -n "$latest_version" ] && [ -d "$latest_version/gcc_64" ]; then
                ENV_QT_PATH="$latest_version"
                break
            fi
        fi
    done
fi

if [ ! -d "$ENV_QT_PATH/gcc_64" ]; then
    echo "error: Qt installation not found at $ENV_QT_PATH/gcc_64"
    exit 1
fi

echo "Using Qt: $ENV_QT_PATH"

script_dir=$(cd "$(dirname "$0")" && pwd)
project_root=$(cd "$script_dir/../.." && pwd)
old_cd=$(pwd)
cd "$project_root"

output_path="./output/x64/$build_mode"
appimage_output_path="./output/appimage"
appdir_path="$appimage_output_path/QtScrcpy.AppDir"
app_name="QtScrcpy"
app_version=$(cat QtScrcpy/appversion 2>/dev/null || echo "0.0.0")

echo "Build mode: $build_mode"
echo "App version: $app_version"

if [ ! -f "$output_path/$app_name" ]; then
    echo "error: $app_name executable not found in $output_path"
    cd "$old_cd"
    exit 1
fi

if [ -d "$appimage_output_path" ]; then
    rm -rf "$appimage_output_path"
fi

mkdir -p "$appimage_output_path"
mkdir -p "$appdir_path/usr/bin"
mkdir -p "$appdir_path/usr/lib"
mkdir -p "$appdir_path/usr/share/applications"
mkdir -p "$appdir_path/usr/share/icons/hicolor/"{16x16,24x24,32x32,48x48,64x64,128x128,256x256}"/apps"
mkdir -p "$appdir_path/usr/share/metainfo"

cp "$output_path/$app_name" "$appdir_path/usr/bin/$app_name"
chmod +x "$appdir_path/usr/bin/$app_name"

if [ -f "$output_path/sndcpy.sh" ]; then
    cp "$output_path/sndcpy.sh" "$appdir_path/usr/bin/"
    chmod +x "$appdir_path/usr/bin/sndcpy.sh"
fi
if [ -f "$output_path/sndcpy.apk" ]; then
    cp "$output_path/sndcpy.apk" "$appdir_path/usr/bin/"
fi

if [ -d "$project_root/keymap" ]; then
    cp -r "$project_root/keymap" "$appdir_path/usr/share/"
fi
if [ -d "$project_root/config" ]; then
    cp -r "$project_root/config" "$appdir_path/usr/share/"
fi

adb_source="$project_root/QtScrcpy/QtScrcpyCore/src/third_party/adb/linux/adb"
server_source="$project_root/QtScrcpy/QtScrcpyCore/src/third_party/scrcpy-server"
mkdir -p "$appdir_path/usr/lib/qtscrcpy"

if [ -f "$adb_source" ]; then
    cp "$adb_source" "$appdir_path/usr/lib/qtscrcpy/adb"
    chmod +x "$appdir_path/usr/lib/qtscrcpy/adb"
    if [ ! -f "$appdir_path/usr/bin/adb" ]; then
        ln -sf "../lib/qtscrcpy/adb" "$appdir_path/usr/bin/adb"
    fi
fi

if [ -f "$server_source" ]; then
    cp "$server_source" "$appdir_path/usr/lib/qtscrcpy/scrcpy-server"
fi

desktop_file="$appdir_path/usr/share/applications/$app_name.desktop"
cat > "$desktop_file" <<EOF
[Desktop Entry]
Type=Application
Name=QtScrcpy
Comment=Android real-time display and control
Exec=QtScrcpy
Icon=QtScrcpy
Categories=Utility;
Terminal=false
EOF

metainfo_file="$appdir_path/usr/share/metainfo/$app_name.appdata.xml"
cat > "$metainfo_file" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>org.qtscrcpy.QtScrcpy</id>
  <metadata_license>MIT</metadata_license>
  <project_license>Apache-2.0</project_license>
  <name>QtScrcpy</name>
  <summary>Android real-time display and control</summary>
  <description>
    <p>QtScrcpy allows you to display and control your Android device via USB or network.</p>
  </description>
  <launchable type="desktop-id">QtScrcpy.desktop</launchable>
</component>
EOF

icon_source="$project_root/QtScrcpy/res/QtScrcpy.png"
if [ -f "$icon_source" ]; then
    for size in 16 24 32 48 64 128 256; do
        cp "$icon_source" "$appdir_path/usr/share/icons/hicolor/${size}x${size}/apps/QtScrcpy.png"
    done
fi

linuxdeployqt_bin="$appimage_output_path/linuxdeployqt"
if [ ! -f "$linuxdeployqt_bin" ]; then
    curl -L "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" -o "$linuxdeployqt_bin"
    chmod +x "$linuxdeployqt_bin"
fi

"$linuxdeployqt_bin" "$desktop_file" -appimage -bundle-non-qt-libs
deploy_status=$?

cd "$old_cd"
exit $deploy_status
