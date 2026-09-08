cmake_minimum_required(VERSION 3.28)

project(AgentBase)

# Add the source files for the AgentBase class
file(GLOB AGENT_BASE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(AgentBase STATIC ${AGENT_BASE_SOURCES})

# Link libraries for the AgentBase library
target_link_libraries(
    ${PROJECT_NAME}
    Messaging
    Calibration
)