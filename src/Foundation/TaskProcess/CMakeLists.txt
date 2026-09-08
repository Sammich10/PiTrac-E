cmake_minimum_required(VERSION 3.28)

project(TaskProcess)

file(GLOB TASK_PROCESS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(TaskProcess STATIC ${TASK_PROCESS_SOURCES})

target_link_libraries(
    ${PROJECT_NAME}
    Messaging
)