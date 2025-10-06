cmake_minimum_required(VERSION 3.28)

project(PiTrac)

file(GLOB STARTAPP_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(PiTrac ${STARTAPP_SOURCES})

target_link_libraries(PiTrac
    pthread
    Json
    FileUtils
)