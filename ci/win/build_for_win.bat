@echo off

echo=
echo=
echo ---------------------------------------------------------------
echo check ENV
echo ---------------------------------------------------------------
echo ENV_QT_PATH %ENV_QT_PATH%

set script_path=%~dp0
set old_cd=%cd%
cd /d %~dp0

set errno=1
goto main

:usage
echo usage: %~nx0 ^<Debug^|Release^|MinSizeRel^|RelWithDebInfo^> ^<x86^|x64^>
goto return

:main

if "%~1"=="" goto usage
if "%~2"=="" goto usage

set build_mode=%~1
set cpu_mode=%~2

if /i "%build_mode%"=="Debug" goto build_mode_ok
if /i "%build_mode%"=="Release" goto build_mode_ok
if /i "%build_mode%"=="MinSizeRel" goto build_mode_ok
if /i "%build_mode%"=="RelWithDebInfo" goto build_mode_ok
echo error: unknown build mode -- %build_mode%
goto usage

:build_mode_ok
if /i "%cpu_mode%"=="x86" (
    set cmake_vs_build_mode=Win32
    set qt_cmake_path=%ENV_QT_PATH%\msvc2019\lib\cmake\Qt5
    goto cpu_mode_ok
)
if /i "%cpu_mode%"=="x64" (
    set cmake_vs_build_mode=x64
    set qt_cmake_path=%ENV_QT_PATH%\msvc2019_64\lib\cmake\Qt5
    goto cpu_mode_ok
)
echo error: unknown cpu mode -- %cpu_mode%
goto usage

:cpu_mode_ok
echo=
echo=
echo ---------------------------------------------------------------
echo begin cmake build
echo ---------------------------------------------------------------
echo current build mode: %build_mode%
echo current cpu mode: %cpu_mode%
echo qt cmake path: %qt_cmake_path%

set output_path=%script_path%..\..\output\%cpu_mode%\%build_mode%
if exist "%output_path%" (
    rmdir /q /s "%output_path%"
)

set temp_path=%script_path%..\build_temp
if exist "%temp_path%" (
    rmdir /q /s "%temp_path%"
)
md "%temp_path%"
cd /d "%temp_path%"

set cmake_params=-DCMAKE_PREFIX_PATH=%qt_cmake_path% -DCMAKE_BUILD_TYPE=%build_mode% -G "Visual Studio 17 2022" -A %cmake_vs_build_mode%
echo cmake params: %cmake_params%

cmake %cmake_params% ../..
if not %errorlevel%==0 (
    echo cmake failed
    goto return
)

cmake --build . --config %build_mode% -j8
if not %errorlevel%==0 (
    echo cmake build failed
    goto return
)

echo=
echo=
echo ---------------------------------------------------------------
echo finish!!!
echo ---------------------------------------------------------------

set errno=0
goto return

:return
cd /d %old_cd%
exit /B %errno%
