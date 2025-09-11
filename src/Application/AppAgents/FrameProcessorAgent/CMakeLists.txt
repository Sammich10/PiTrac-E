project(FrameProcessorAgent)

file(GLOB FRAME_PROCESSOR_AGENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${PROJECT_NAME} SHARED
    ${FRAME_PROCESSOR_AGENT_SOURCES}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    Messaging
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)
