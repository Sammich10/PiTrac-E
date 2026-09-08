cmake_minimum_required(VERSION 3.28)

# Named distinctly from Common/Utils/Calibration's "Calibration" target to avoid
# output-file collisions on case-insensitive filesystems (e.g. WSL /mnt/c).
project(LegacyCalibration)

# Glob all source files
file(GLOB CALIBRATION_SOURCE 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)


# Add the shared library
add_library(${PROJECT_NAME} SHARED ${CALIBRATION_SOURCE})

# Link OpenCV libraries
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})