cmake_minimum_required(VERSION 3.28)

project(CameraAgent)

# Add the source files for the CameraAgent class
file(GLOB CAMERA_AGENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the static library for CameraAgent
add_library(CameraAgent STATIC ${CAMERA_AGENT_SOURCES})

# Link OpenCV libraries
target_link_libraries(
    ${PROJECT_NAME} 
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
    ${SQLite3_LIBRARIES}
    Calibration
    FileUtils
)