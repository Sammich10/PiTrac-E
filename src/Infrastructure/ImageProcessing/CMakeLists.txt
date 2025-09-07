cmake_minimum_required(VERSION 3.28)

project(ImageProcessor)

file(GLOB IMAGE_PROCESSOR_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EdgeDrawing/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EdgeDrawing/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EllipseDetector/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/EllipseDetector/*.h
)

add_library(ImageProcessor SHARED ${IMAGE_PROCESSOR_SOURCES})

target_link_libraries(ImageProcessor 
    ${OpenCV_LIBS} 
    CVUtils
    ColorSys
)
