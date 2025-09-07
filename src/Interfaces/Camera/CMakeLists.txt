cmake_minimum_required(VERSION 3.28)

project(GSCameraInterface)

file(GLOB CAMERA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Add the source files to the project
add_library(${PROJECT_NAME} INTERFACE)

# Link the necessary libraries
target_link_libraries(${PROJECT_NAME} INTERFACE
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)

add_subdirectory(imx296)
add_subdirectory(GSCameraBase)
