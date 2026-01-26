project(CameraAgent)

file(GLOB CAMERA_AGENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(CameraAgent ${CAMERA_AGENT_SOURCES})

target_link_libraries(CameraAgent
    PUBLIC
    Messaging
    GSCameraInterface
    GSCameraBase
    imx296
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)
