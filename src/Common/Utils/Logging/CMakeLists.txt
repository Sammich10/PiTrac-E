# CMake configuration for the Logging utilities library
project(Logging)

find_package(Boost REQUIRED 
    log
    log_setup
    filesystem
)

# Add the source files for the Logging utilities
file(GLOB LOGGING_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

# Add the shared library
add_library(${PROJECT_NAME} SHARED ${LOGGING_SOURCES})

# Link boost logging libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE
    Boost::log
    Boost::log_setup
    Boost::filesystem
)