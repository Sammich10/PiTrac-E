project(SystemTask)

file(GLOB ${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(${PROJECT_NAME} ${${PROJECT_NAME}_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        GSManagerTask
        GSTaskBase
        Messaging
        Logging
        GSManagerBase
        SystemManager
        FileUtils
)