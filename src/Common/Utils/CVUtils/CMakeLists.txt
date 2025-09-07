cmake_minimum_required(VERSION 3.28)

# Set project name
project(CVUtils)

# Add the source files for CVUtils
file(GLOB CV_UTILS_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(${PROJECT_NAME} SHARED ${CV_UTILS_SOURCES})

# Link OpenCV libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS} 
    ColorSys
)
