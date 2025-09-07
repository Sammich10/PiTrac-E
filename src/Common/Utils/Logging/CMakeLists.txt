cmake_minimum_required(VERSION 3.28)

project(Logging)

# Add the source files for the Logging utilities
file(GLOB LOGGING_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(${PROJECT_NAME} SHARED ${LOGGING_SOURCES})

# Link OpenCV libraries
target_link_libraries(${PROJECT_NAME} 
    ${OpenCV_LIBS}
    ${Boost_LIBRARIES}
)
