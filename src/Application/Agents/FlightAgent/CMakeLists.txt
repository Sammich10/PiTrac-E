project(FlightAgent)

file(GLOB FLIGHT_AGENT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(FlightAgent ${FLIGHT_AGENT_SOURCES})

target_link_libraries(FlightAgent
    PUBLIC
    Messaging
    GSCameraInterface
    GSCameraBase
    imx296
    CameraAgent
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)
