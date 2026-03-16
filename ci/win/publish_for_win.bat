@echo off

echo=
echo=
echo ---------------------------------------------------------------
echo check ENV
echo ---------------------------------------------------------------

set vcvarsall="%ENV_VCVARSALL%"
set qt_msvc_path="%ENV_QT_PATH%"

echo ENV_VCVARSALL %ENV_VCVARSALL%
echo ENV_QT_PATH %ENV_QT_PATH%

set script_path=%~dp0
set old_cd=%cd%
cd /d %~dp0

set errno=1
goto main

:usage
echo usage: %~nx0 ^<x86^|x64^> ^<publish_dir^> ^<Debug^|Release^|MinSizeRel^|RelWithDebInfo^>
goto return

:main

if "%~1"=="" goto usage
if "%~2"=="" goto usage
if "%~3"=="" goto usage

set cpu_mode=%~1
set publish_dir=%~2
set build_mode=%~3

if /i "%cpu_mode%"=="x86" goto cpu_mode_ok
if /i "%cpu_mode%"=="x64" goto cpu_mode_ok
echo error: unknown cpu mode -- %cpu_mode%
goto usage

:cpu_mode_ok
if /i "%build_mode%"=="Debug" goto build_mode_ok
if /i "%build_mode%"=="Release" goto build_mode_ok
if /i "%build_mode%"=="MinSizeRel" goto build_mode_ok
if /i "%build_mode%"=="RelWithDebInfo" goto build_mode_ok
echo error: unknown build mode -- %build_mode%
goto usage

:build_mode_ok
set adb_path=%script_path%..\..\QtScrcpy\QtScrcpyCore\src\third_party\adb\win\*.*
set jar_path=%script_path%..\..\QtScrcpy\QtScrcpyCore\src\third_party\scrcpy-server
set keymap_path=%script_path%..\..\keymap
set config_path=%script_path%..\..\config
set publish_path=%script_path%%publish_dir%\

if /i "%cpu_mode%"=="x86" (
    set release_path=%script_path%..\..\output\x86\%build_mode%
    set qt_msvc_path=%qt_msvc_path%\msvc2019\bin
) else (
    set release_path=%script_path%..\..\output\x64\%build_mode%
    set qt_msvc_path=%qt_msvc_path%\msvc2019_64\bin
)
set PATH=%qt_msvc_path%;%PATH%

echo current cpu mode: %cpu_mode%
echo current build mode: %build_mode%
echo current publish dir: %publish_dir%

if not exist "%release_path%" (
    echo error: build output not found -- %release_path%
    goto return
)

call %vcvarsall% %cpu_mode%
if not %errorlevel%==0 goto return

if exist "%publish_path%" (
    rmdir /s /q "%publish_path%"
)

xcopy "%release_path%" "%publish_path%" /E /Y /I
if not %errorlevel%==0 goto return
xcopy "%adb_path%" "%publish_path%" /Y
if not %errorlevel%==0 goto return
xcopy "%jar_path%" "%publish_path%" /Y
if not %errorlevel%==0 goto return
xcopy "%keymap_path%" "%publish_path%keymap\" /E /Y /I
if not %errorlevel%==0 goto return
xcopy "%config_path%" "%publish_path%config\" /E /Y /I
if not %errorlevel%==0 goto return

windeployqt "%publish_path%\QtScrcpy.exe"
if not %errorlevel%==0 goto return

rmdir /s /q "%publish_path%\iconengines"
rmdir /s /q "%publish_path%\translations"

del "%publish_path%\imageformats\qgif.dll"
del "%publish_path%\imageformats\qicns.dll"
del "%publish_path%\imageformats\qico.dll"
del "%publish_path%\imageformats\qsvg.dll"
del "%publish_path%\imageformats\qtga.dll"
del "%publish_path%\imageformats\qtiff.dll"
del "%publish_path%\imageformats\qwbmp.dll"
del "%publish_path%\imageformats\qwebp.dll"

if /i "%cpu_mode%"=="x86" (
    del "%publish_path%\vc_redist.x86.exe"
) else (
    del "%publish_path%\vc_redist.x64.exe"
)

if /i "%cpu_mode%"=="x64" (
    copy /Y "C:\Windows\System32\msvcp140_1.dll" "%publish_path%\msvcp140_1.dll" >nul
    if not %errorlevel%==0 goto return
    copy /Y "C:\Windows\System32\msvcp140.dll" "%publish_path%\msvcp140.dll" >nul
    if not %errorlevel%==0 goto return
    copy /Y "C:\Windows\System32\vcruntime140.dll" "%publish_path%\vcruntime140.dll" >nul
    if not %errorlevel%==0 goto return
    copy /Y "C:\Windows\System32\vcruntime140_1.dll" "%publish_path%\vcruntime140_1.dll" >nul
    if not %errorlevel%==0 goto return
) else (
    copy /Y "C:\Windows\SysWOW64\msvcp140_1.dll" "%publish_path%\msvcp140_1.dll" >nul
    if not %errorlevel%==0 goto return
    copy /Y "C:\Windows\SysWOW64\msvcp140.dll" "%publish_path%\msvcp140.dll" >nul
    if not %errorlevel%==0 goto return
    copy /Y "C:\Windows\SysWOW64\vcruntime140.dll" "%publish_path%\vcruntime140.dll" >nul
    if not %errorlevel%==0 goto return
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
