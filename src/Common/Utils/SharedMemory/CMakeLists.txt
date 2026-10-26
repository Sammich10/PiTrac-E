# CMake configuration for the SharedMemory library within the Common/Utils module
project(SharedMemory)

# Find boost package for boost interprocess library
find_package(Boost REQUIRED)
find_package(Threads REQUIRED)

# SharedMemoryObject/CircularFrameBuffer are header-only templates; expose as an INTERFACE library
add_library(SharedMemory INTERFACE)
target_sources(SharedMemory INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/SharedMemory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/CircularFrameBuffer.h
)
target_include_directories(SharedMemory INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(SharedMemory INTERFACE Boost::boost rt Threads::Threads)