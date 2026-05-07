cmake_minimum_required(VERSION 3.28)

project(FlightProcessor)

# Add the source files for the FlightProcessor class
file(GLOB FLIGHT_PROCESSOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_executable(FlightProcessor ${FLIGHT_PROCESSOR_SOURCES})

# Link OpenCV libraries
target_link_libraries(
    ${PROJECT_NAME} 
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
    ${SQLite3_LIBRARIES}
    Calibration
    FileUtils
    Detection
    Messaging
    GSCameraInterface
    GSCameraBase
    imx296
)