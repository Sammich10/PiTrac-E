# Name the project
project(System)

# Find boost interprocess libraries
find_package(Boost REQUIRED COMPONENTS filesystem system)

file(GLOB ${PROJECT_NAME}_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(${PROJECT_NAME} SHARED
${${PROJECT_NAME}_SOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem Boost::system)