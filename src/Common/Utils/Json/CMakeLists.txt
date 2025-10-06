project(Json)

file(GLOB JSON_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
)

add_library(Json SHARED ${JSON_SOURCES})

target_link_libraries(Json
    jsoncpp
)