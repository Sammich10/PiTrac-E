project(CameraTask)

file(GLOB CAMERA_TASK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(${PROJECT_NAME} ${CAMERA_TASK_SOURCES})

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        GSAgentTask
        GSTaskBase
        Messaging
        Logging
        GSAgentBase
        CameraAgent
        FrameProcessorAgent
        GSCameraInterface
        GSCameraBase
        imx296
        CVUtils
        FileUtils
        ColorSys
        ${OpenCV_LIBS}
        ${LIBCAMERA_LIBS}
)