if(NOT DEFINED QSC_TARGET_FILE OR NOT EXISTS "${QSC_TARGET_FILE}")
    message(FATAL_ERROR "[QtScrcpy] Missing target file for Qt deployment: ${QSC_TARGET_FILE}")
endif()

if(NOT DEFINED QSC_DEPLOY_DIR OR QSC_DEPLOY_DIR STREQUAL "")
    message(FATAL_ERROR "[QtScrcpy] Missing deploy directory for Qt deployment")
endif()

if(NOT DEFINED QSC_WINDEPLOYQT_EXECUTABLE OR NOT EXISTS "${QSC_WINDEPLOYQT_EXECUTABLE}")
    message(FATAL_ERROR "[QtScrcpy] Missing windeployqt executable: ${QSC_WINDEPLOYQT_EXECUTABLE}")
endif()

set(qt_suffix "")
if(DEFINED QSC_DEPLOY_CONFIG AND QSC_DEPLOY_CONFIG STREQUAL "Debug")
    set(qt_suffix "d")
endif()

set(qsc_required_runtime_files
    "Qt6Core${qt_suffix}.dll"
    "Qt6Gui${qt_suffix}.dll"
    "Qt6Widgets${qt_suffix}.dll"
    "Qt6Network${qt_suffix}.dll"
    "Qt6Multimedia${qt_suffix}.dll"
    "Qt6OpenGL${qt_suffix}.dll"
    "Qt6OpenGLWidgets${qt_suffix}.dll"
    "platforms/qwindows${qt_suffix}.dll"
)

set(qsc_missing_runtime_files "")
foreach(runtime_file IN LISTS qsc_required_runtime_files)
    if(NOT EXISTS "${QSC_DEPLOY_DIR}/${runtime_file}")
        list(APPEND qsc_missing_runtime_files "${runtime_file}")
    endif()
endforeach()

if(NOT qsc_missing_runtime_files)
    message(STATUS "[QtScrcpy] Qt runtime already present for ${QSC_DEPLOY_CONFIG}, skip windeployqt")
    return()
endif()

string(JOIN ", " qsc_missing_runtime_files_text ${qsc_missing_runtime_files})
message(STATUS "[QtScrcpy] Missing Qt runtime files for ${QSC_DEPLOY_CONFIG}: ${qsc_missing_runtime_files_text}")
message(STATUS "[QtScrcpy] Running windeployqt for ${QSC_TARGET_FILE}")

execute_process(
    COMMAND "${QSC_WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime "${QSC_TARGET_FILE}"
    RESULT_VARIABLE qsc_windeployqt_result
)

if(NOT qsc_windeployqt_result EQUAL 0)
    message(FATAL_ERROR "[QtScrcpy] windeployqt failed with exit code ${qsc_windeployqt_result}")
endif()
