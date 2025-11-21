cmake_minimum_required(VERSION 3.28)

project(Calibration)

# Add the source files for the Calibration utilities
file(GLOB CALIBRATION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(Calibration SHARED ${CALIBRATION_SOURCES})

# Link OpenCV libraries
target_link_libraries(
    ${PROJECT_NAME} 
    ${OpenCV_LIBS} 
    ${SQLite3_LIBRARIES}
)