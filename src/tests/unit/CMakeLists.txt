project(PiTracUnitTests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testbin/unit)

# Find Google Test
find_package(GTest REQUIRED)

# Add test subdirectories
add_subdirectory(Common/Utils/ColorSys)
add_subdirectory(Common/Utils/CV)
add_subdirectory(Common/Utils/Logging)
add_subdirectory(Common/Utils/FileUtils)
