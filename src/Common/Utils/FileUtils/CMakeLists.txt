cmake_minimum_required(VERSION 3.28)

project(FileUtils)

# Add the source files for the FileUtils
file(GLOB FILE_UTILS_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(${PROJECT_NAME} SHARED ${FILE_UTILS_SOURCES})

