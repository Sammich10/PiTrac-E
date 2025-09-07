cmake_minimum_required(VERSION 3.28)

project(startapp)

file(GLOB STARTAPP_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${PROJECT_NAME} ${STARTAPP_SOURCES})
