cmake_minimum_required(VERSION 3.28)

project(PiTracTests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testbin)

# Add unit tests
add_subdirectory(unit)
add_subdirectory(functional)
