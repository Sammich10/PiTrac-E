project(I2CUtils)

file(GLOB I2CUTILS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Find OpenSSL for MD5 functionality
find_package(OpenSSL REQUIRED)

add_library(${PROJECT_NAME} STATIC ${I2CUTILS_SOURCES})
target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::Crypto)