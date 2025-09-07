cmake_minimum_required(VERSION 3.28)

project(ColorSys)

# Add the source files for the ColorSys utilities
file(GLOB COLOR_SYS_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(${PROJECT_NAME} SHARED ${COLOR_SYS_SOURCES})

# Link OpenCV libraries
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})
