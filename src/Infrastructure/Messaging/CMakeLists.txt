project(Messaging)

file(GLOB MESSAGING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Messages/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Messages/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Messagers/*.h
)

add_library(Messaging SHARED ${MESSAGING_SOURCES})

target_link_libraries(
    Messaging
    PUBLIC
        Logging
        zmq
)