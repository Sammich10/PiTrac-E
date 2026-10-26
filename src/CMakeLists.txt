
# CMake configuration for the PiTrac project
cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project configuration for PiTracEmbedded
project(PiTracEmbedded)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Set the include directory for the project
include_directories(${CMAKE_SOURCE_DIR})

# Find PkgConfig for managing library dependencies
find_package(PkgConfig REQUIRED)

# Find OpenCV
find_package(OpenCV REQUIRED core imgproc highgui videoio calib3d aruco features2d objdetect video)
if(NOT OpenCV_FOUND)
    # If OpenCV is not found, print an error message
    message(FATAL_ERROR "OpenCV not found. Please install OpenCV or set the OpenCV_DIR environment variable.")
    # Stop processing further
    return()
else()
    include_directories(${OpenCV_INCLUDE_DIRS})
    # Use SYSTEM to suppress warnings from OpenCV headers
    include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
endif()

# Find libcamera
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
if(NOT LIBCAMERA_FOUND)
    message(FATAL_ERROR "libcamera not found. Please install libcamera or set the LIBCAMERA_DIR environment variable.")
    return()
else()
    include_directories(${LIBCAMERA_INCLUDE_DIRS})
    set(LIBCAMERA_LIBS ${LIBCAMERA_LIBRARIES})
endif()

# Add the output directory for generated build files to the include path (for generated headers, primarily generated IPC messages)
include_directories(${CMAKE_BINARY_DIR})

# Add subdirectories
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Foundation)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Infrastructure)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Interfaces)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Calibration)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Application)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Startup)

