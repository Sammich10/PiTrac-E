cmake_minimum_required(VERSION 3.28)

# Set project name
project(ImageAnalysis)

# Add the files for ImageAnalysis
file(GLOB IMAGE_ANALYSIS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/application/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/domain/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

# Add the source files to the project
add_library(${PROJECT_NAME} INTERFACE)

# Add include directories
target_include_directories(${PROJECT_NAME} INTERFACE ${IMAGE_ANALYSIS_SOURCES})

# Link the necessary libraries
target_link_libraries(${PROJECT_NAME} INTERFACE
    ${OpenCV_LIBS}
)
