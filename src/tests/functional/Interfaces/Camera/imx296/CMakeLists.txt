project(imx296_tests)

file(GLOB IMX296_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../*.h
)

add_executable(imx296_test ${IMX296_TEST_SOURCES})

target_link_libraries(imx296_test
    PRIVATE
    GSCameraInterface
    GSCameraBase
    imx296
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)