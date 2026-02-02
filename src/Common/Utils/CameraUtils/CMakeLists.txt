cmake_minimum_required(VERSION 3.28)

project(CameraUtils)

# Add the source files for the Calibration utilities
file(GLOB CAMERA_UTILS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(CameraUtils SHARED ${CAMERA_UTILS_SOURCES})

# Link OpenCV libraries
target_link_libraries(
    ${PROJECT_NAME} 
    ${OpenCV_LIBS} 
    ${LIBCAMERA_LIBS}
)