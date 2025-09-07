# Add the test executable
add_executable(test_file_utils
    test_file_utils.cpp
)

# Link against the colorsys library, gtest, and OpenCV
target_link_libraries(test_file_utils
    PRIVATE
    FileUtils # Link to the FileUtils library
    GTest::gtest_main
)

# Register the test with CTest
add_test(NAME FileUtilsUnitTests COMMAND test_file_utils)
