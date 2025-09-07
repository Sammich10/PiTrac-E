project(gscamerabase_test)

file(GLOB GSCAMERABASE_TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../*.h
)

add_executable(gscamerabase_test ${GSCAMERABASE_TEST_SOURCES})

target_link_libraries(gscamerabase_test
    PRIVATE
    GSCameraInterface
    GSCameraBase
    imx296
    ${OpenCV_LIBS}
    ${LIBCAMERA_LIBS}
)