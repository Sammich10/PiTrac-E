# Add the test executable
add_executable(test_colorsys
    test_colorsys.cpp
)

# Link against the colorsys library, gtest, and OpenCV
target_link_libraries(test_colorsys
    PRIVATE
    ColorSys # Link to the ColorSys library
    GTest::gtest_main
    ${OpenCV_LIBS}
)

# Register the test with CTest
add_test(NAME ColorsysUnitTests COMMAND test_colorsys)
