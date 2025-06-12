cmake_minimum_required(VERSION 3.30)
project(WaveOCR)

set(CMAKE_CXX_STANDARD 20)

set(OpenCV_DIR "/usr/share/OpenCV")
find_package(OpenCV REQUIRED)

include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(src)

add_subdirectory(pybind11-2.13.6)

add_library(WaveOCRLib
        src/ocr.cpp
        src/generator.cpp
)

set_target_properties(WaveOCRLib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(WaveOCRLib PRIVATE ${OpenCV_LIBS})

pybind11_add_module(WaveOCR main.cpp)
target_link_libraries(WaveOCR PRIVATE WaveOCRLib ${OpenCV_LIBS})

