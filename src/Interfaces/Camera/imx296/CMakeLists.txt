project(imx296)

file(GLOB IMX296_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Add the source files to the project
add_library(${PROJECT_NAME} STATIC ${IMX296_SOURCES})

# Link the necessary libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
    GSCameraInterface
    GSCameraBase
)
