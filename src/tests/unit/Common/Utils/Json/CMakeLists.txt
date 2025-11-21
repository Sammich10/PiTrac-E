# Add the test executable
add_executable(test_json_parser
    test_json_parser.cpp
)

# Link against the jsonparser library and gtest
target_link_libraries(test_json_parser
    PRIVATE
    Json # Link to the JsonParser library
    GTest::gtest_main
)

# Register the test with CTest
add_test(NAME JsonParserUnitTests COMMAND test_json_parser)