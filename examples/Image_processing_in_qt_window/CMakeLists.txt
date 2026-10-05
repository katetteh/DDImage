find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(ImageProcessorApp main.cpp)

target_link_libraries(ImageProcessorApp PRIVATE
    ddimage
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

target_compile_definitions(ImageProcessorApp PRIVATE
    DDIMAGE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../data")

if(APPLE)
    target_link_options(ImageProcessorApp PRIVATE
        "LINKER:-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_SOURCE_DIR}/info.plist")
endif()