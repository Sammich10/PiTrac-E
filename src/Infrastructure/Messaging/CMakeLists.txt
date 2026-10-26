# Name the project
project(Messaging)

# Find msgpack headers
find_package(msgpack-cxx REQUIRED)

# Include directories and link libraries for msgpack-cxx
include_directories(${msgpack-cxx_INCLUDE_DIRS})

# Gather all source and header files for the Messaging library
file(GLOB MESSAGING_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/Messages/*.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Messages/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.h
)

# Create the Messaging library from the gathered source and header files
add_library(${PROJECT_NAME} SHARED ${MESSAGING_SOURCES})

# Link the Messaging library against the required libraries
target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    zmq
    PRIVATE
    ${msgpack-cxx_LIBRARIES}
    Logging
)