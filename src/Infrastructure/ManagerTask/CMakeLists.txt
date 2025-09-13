project(GSManagerTask)

file(GLOB ${PROJECT_NAME}_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${PROJECT_NAME} SHARED ${${PROJECT_NAME}_SOURCE_FILES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    Logging
    GSTaskBase
)