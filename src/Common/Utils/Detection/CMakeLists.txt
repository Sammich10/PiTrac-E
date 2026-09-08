project(Detection)

file(GLOB DETECTION_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_library(
    ${PROJECT_NAME} SHARED ${DETECTION_SOURCES}
)
target_link_libraries(
    ${PROJECT_NAME} PUBLIC 
    ${OpenCV_LIBS}
)