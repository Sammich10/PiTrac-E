# Add the test executable
add_executable(test_gslogger
    test_gslogger.cpp
)

# Link against the colorsys library, gtest, and OpenCV
target_link_libraries(test_gslogger
    PUBLIC
    Logging
    CVUtils
    GTest::gtest_main
    ${OpenCV_LIBS}
    ${BOOST_LIBS}
    Boost::log
    Boost::log_setup
)

# Register the test with CTest
add_test(NAME GSLoggerUnitTests COMMAND test_gslogger)
