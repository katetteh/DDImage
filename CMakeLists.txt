cmake_minimum_required(VERSION 3.16)
project(ImageProcessorProject)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt auto-generation tools (required for Qt macros like Q_OBJECT)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find the Qt Widgets library (change Qt6 to Qt5 if you are on an older version)
find_package(Qt6 COMPONENTS Widgets REQUIRED)

# Define your source files
set(SOURCES
    main.cpp
    ImageProcessor.cpp
    matrix.cpp
)

# Create the executable
add_executable(ImageProcessorApp ${SOURCES})

# Link the Qt Widgets module to your application
target_link_libraries(ImageProcessorApp PRIVATE Qt6::Widgets)