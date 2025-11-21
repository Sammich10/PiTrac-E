#include "Common/Utils/Json/jsonparser.h"
#include "Common/Utils/Json/structdef/CalConfig.h"
#include <iostream>
#include <fstream>
#include <gtest/gtest.h>

using namespace PiTrac;

class JsonParserTest : public ::testing::Test
{
  protected:
    std::string validConfigPath = "test_valid_config.json";
    std::string invalidConfigPath = "test_invalid_config.json";

    void SetUp() override
    {
        // Create a valid JSON config file
        std::ofstream validFile(validConfigPath);
        validFile << R"({
            "SchemaVersion": "1.0",
            "SchemaFile": "schema.sql",
            "DatabaseFile": "calibration.db"
        })";
        validFile.close();

        // Create an invalid JSON config file
        std::ofstream invalidFile(invalidConfigPath);
        invalidFile << R"({
            "SchemaVersion": 1.0,
            "SchemaFile": "schema.sql"
        })"; // Missing DatabaseFile and wrong type for SchemaVersion
        invalidFile.close();
    }

    void TearDown() override
    {
        // Remove test files
        std::remove(validConfigPath.c_str());
        std::remove(invalidConfigPath.c_str());
    }
};

TEST_F(JsonParserTest, ParseValidCalConfig)
{
    EXPECT_NO_THROW({
        CalConfig config = JsonParser::parseCalConfig(validConfigPath);
        EXPECT_EQ(config.SchemaVersionMajor, 1);
        EXPECT_EQ(config.SchemaVersionMinor, 0);
        EXPECT_EQ(config.SchemaFile, "schema.sql");
        EXPECT_EQ(config.DatabaseFile, "calibration.db");
    });
}

TEST_F(JsonParserTest, ParseInvalidCalConfig)
{
    EXPECT_THROW({
        CalConfig config = JsonParser::parseCalConfig(invalidConfigPath);
    }, std::runtime_error);
}