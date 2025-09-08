project(Messaging)

file(GLOB MESSAGING_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Messages/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Messages/*.h
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
    ${OpenCV_LIBS}
    ${BOOST_LIBS}
)