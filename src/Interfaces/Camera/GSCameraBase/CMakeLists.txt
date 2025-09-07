project(GSCameraBase)

file(GLOB GSCAMERA_BASE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Add the source files to the project
add_library(${PROJECT_NAME} STATIC ${GSCAMERA_BASE_SOURCES})

# Link the necessary libraries
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
    GSCameraInterface
)
