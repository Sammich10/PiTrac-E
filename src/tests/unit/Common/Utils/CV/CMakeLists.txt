# Add the test executable
add_executable(test_cv_utils
    test_cv_utils.cpp
)

# Link against the colorsys library, gtest, and OpenCV
target_link_libraries(test_cv_utils
    PRIVATE
    CVUtils # Link to the CVUtils library
    GTest::gtest_main
    ${OpenCV_LIBS}
)

# Register the test with CTest
add_test(NAME CVUtilsUnitTests COMMAND test_cv_utils)
