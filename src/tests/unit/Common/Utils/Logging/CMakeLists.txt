# Add the test executable
add_executable(test_gslogger
    test_gslogger.cpp
)

# Link against the colorsys library, gtest, and OpenCV
target_link_libraries(test_gslogger
    PRIVATE
    Logging # Link to the Logging library
    CVUtils
    GTest::gtest_main
    Boost::log
    Boost::log_setup
    Boost::system
    Boost::filesystem
    Boost::thread
    Boost::chrono
    Boost::atomic
    ${OpenCV_LIBS}
)

# Register the test with CTest
add_test(NAME GSLoggerUnitTests COMMAND test_gslogger)
