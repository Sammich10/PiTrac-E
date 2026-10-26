cmake_minimum_required(VERSION 3.28)

project(Calibration)

# Find SQLite3
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found. Please install SQLite3 or set the SQLite3_DIR environment variable.")
    return()
else()
    include_directories(${SQLite3_INCLUDE_DIRS})
endif()

# Add the source files for the Calibration utilities
file(GLOB CALIBRATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Detectors/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Detectors/*.h
)

# Add the shared library
add_library(Calibration SHARED ${CALIBRATION_SOURCES})

target_link_libraries(
    ${PROJECT_NAME} 
    PRIVATE
    ${OpenCV_LIBS}
    ${SQLite3_LIBRARIES}
    Logging
    FileUtils
    Json
)