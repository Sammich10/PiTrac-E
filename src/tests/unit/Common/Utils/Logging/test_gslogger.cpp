#include <gtest/gtest.h>
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac {

class GSLoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger with INFO level
        logger = new GSLogger(PiTrac::logger_level::info);
    }
    
    void TearDown() override {
        delete logger;
        // Optionally, remove the test log file after tests
        std::remove("test_log.log");
    }

    GSLogger* logger;
};

// Test logging at different levels
TEST_F(GSLoggerTest, LogAtDifferentLevels) {
    // These calls should log messages without crashing
    logger->trace("This is a trace message");
    logger->debug("This is a debug message");
    logger->info("This is an info message");
    logger->warning("This is a warning message");
    logger->error("This is an error message");
    logger->fatal("This is a fatal message");

    // Since we cannot easily check the log file contents here,
    // we assume if no exceptions were thrown, logging works.
}

// Test formatted logging
TEST_F(GSLoggerTest, FormattedLogging) {
    logger->info("Formatted number: %d, string: %s", 42, "test");
    logger->error("Error code: %d", -1);
    
    // Again, we assume if no exceptions were thrown, formatted logging works.
}

// Test setting log level at runtime
TEST_F(GSLoggerTest, SetLogLevel) {
    logger->setLogLevel(PiTrac::logger_level::error);
    
    // This debug message should not be logged
    logger->debug("This debug message should not appear in the log");
    
    // This error message should be logged
    logger->error("This error message should appear in the log");
    
    // If no exceptions were thrown, setting log level works.
}

}  // namespace PiTrac