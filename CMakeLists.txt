cmake_minimum_required(VERSION 3.16)
project(DDImage VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(DDIMAGE_BUILD_EXAMPLES "Build example apps" ON)
option(DDIMAGE_BUILD_TESTS "Build tests" OFF)

add_library(ddimage STATIC src/matrix.cpp src/ImageProcessor.cpp)
target_include_directories(ddimage PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(NOT MSVC)
    target_compile_options(ddimage PRIVATE -Wno-deprecated-declarations)
endif()

if(DDIMAGE_BUILD_EXAMPLES)
    add_subdirectory(examples/Image_processing_in_qt_window)
endif()

if(DDIMAGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()