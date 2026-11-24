find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Test Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets)

add_executable(compatibility_tests compatibility_tests.cpp)
set_target_properties(compatibility_tests PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(compatibility_tests PRIVATE
    "${PROJECT_SOURCE_DIR}/QtScrcpy/QtScrcpyCore/src/common"
    "${PROJECT_SOURCE_DIR}/QtScrcpy/QtScrcpyCore/src/device/android"
    "${PROJECT_SOURCE_DIR}/QtScrcpy/QtScrcpyCore/src/device/controller"
    "${PROJECT_SOURCE_DIR}/QtScrcpy/QtScrcpyCore/src/device/controller/inputconvert"
)
target_link_libraries(compatibility_tests PRIVATE
    QtScrcpyCore
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
)

add_test(NAME compatibility_regressions COMMAND compatibility_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/compatibility-results-$<CONFIG>.xml,junitxml"
)
if(WIN32)
    set_tests_properties(compatibility_regressions PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:QtScrcpy>;PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Test>"
    )
endif()
