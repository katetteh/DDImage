cmake_minimum_required(VERSION 3.16)
project(ImageProcessorProject)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt auto-generation tools (required for Qt macros like Q_OBJECT)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find the Qt libraries (Multimedia + MultimediaWidgets are needed for the camera)
find_package(Qt6 COMPONENTS Widgets Multimedia MultimediaWidgets REQUIRED)

# Define your source files
set(SOURCES
    main.cpp
    ImageProcessor.cpp
    matrix.cpp
)

# Plain executable on every OS (no MACOSX_BUNDLE)
add_executable(ImageProcessorApp ${SOURCES})

# macOS only: embed Info.plist into the binary so Qt can find the camera
# usage description. Windows and Linux ignore this block.
if(APPLE)
    target_link_options(ImageProcessorApp PRIVATE
        "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/Info.plist")
endif()

target_link_libraries(ImageProcessorApp PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)