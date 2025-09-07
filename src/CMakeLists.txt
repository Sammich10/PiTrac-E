cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(PiTrac)

# Set the include directory for the project
include_directories(${CMAKE_SOURCE_DIR})

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find boost libraries
find_package(Boost REQUIRED
    system
    filesystem
    thread
    chrono
    atomic
    log
    log_setup
)

set(BOOST_LIBS
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::chrono
    Boost::atomic
    Boost::log
    Boost::log_setup
)

# Define LIBAV_PRESENT if libav libraries are found
find_library(LIBAV avcodec QUIET)
if(LIBAV)
    message(STATUS "Found libav: ${LIBAV}")
    set(LIBAV_PRESENT TRUE)
else()
    message(STATUS "libav not found")
    set(LIBAV_PRESENT FALSE)
endif()
# If LIBAV_PRESENT is TRUE, include the directories and link the libraries
if(LIBAV_PRESENT)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBAV REQUIRED libavcodec libavformat libavutil libavfilter libavdevice libswscale libswresample libavresample)
    include_directories(${LIBAV_INCLUDE_DIRS})
    link_directories(${LIBAV_LIBRARY_DIRS})
    set(LIBAV_LIBS ${LIBAV_LIBRARIES})
else()
    message(FATAL_ERROR "libav not found.")
endif()

# Find OpenCV
find_package(OpenCV REQUIRED core imgproc highgui videoio)
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
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCAMERA REQUIRED libcamera)
if(NOT LIBCAMERA_FOUND)
    message(FATAL_ERROR "libcamera not found. Please install libcamera or set the LIBCAMERA_DIR environment variable.")
    return()
else()
    include_directories(${LIBCAMERA_INCLUDE_DIRS})
    set(LIBCAMERA_LIBS ${LIBCAMERA_LIBRARIES})
endif()

# Find msgpack headers
find_package(msgpack-cxx REQUIRED)
if(NOT msgpack-cxx_FOUND)
    message(FATAL_ERROR "msgpack-cxx not found. Please install msgpack-cxx or set the MSGPACK_CXX_DIR environment variable.")
    return()
else()
    include_directories(${msgpack-cxx_INCLUDE_DIRS})
    set(MSGPACK_CXX_LIBS ${msgpack-cxx_LIBRARIES})
endif()

# add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/startapp)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Calibration)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Common)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Interfaces)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/Infrastructure)

option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    message(STATUS "Building unit tests")
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()