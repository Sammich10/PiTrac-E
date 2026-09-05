project(Messaging)

file(GLOB MESSAGING_SOURCES
    ${CMAKE_CURRENT_BINARY_DIR}/Messages/*.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/Messages/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.h
)

add_library(${PROJECT_NAME} SHARED ${MESSAGING_SOURCES})

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    Logging
    zmq
    msgpack-cxx
    ${BOOST_LIBS}
)