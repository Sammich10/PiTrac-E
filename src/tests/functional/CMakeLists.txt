project(PiTracFunctionalTests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testbin/func)

# Add test subdirectories
add_subdirectory(Interfaces/Camera/imx296)
add_subdirectory(Interfaces/Camera/GSCameraBase)
