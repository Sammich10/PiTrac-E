project(TeeAgent)

file(GLOB TEE_AGENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(TeeAgent ${TEE_AGENT_SOURCES})

target_link_libraries(TeeAgent
    PUBLIC
    Messaging
    GSCameraInterface
    GSCameraBase
    imx296
    CameraAgent
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)
