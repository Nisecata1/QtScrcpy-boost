cmake_minimum_required(VERSION 3.19 FATAL_ERROR)
project(all)

add_subdirectory(QtScrcpy)

option(QSC_BUILD_TESTS "Build compatibility regression tests" OFF)
if(QSC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
