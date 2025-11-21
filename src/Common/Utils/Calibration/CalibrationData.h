#ifndef __CLIBRATION_DATA_H__
#define __CLIBRATION_DATA_H__

#include <array>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <opencv2/opencv.hpp>
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac
{
/**
 * @brief CalibrationData is a singleton class used to execute SQL queries on
 * the calibration database.
 */
class CalibrationData
{
  public:
/**
 * @brief Get the singleton instance of CalibrationData
 * @return Reference to the CalibrationData instance
 */
    static std::shared_ptr<CalibrationData> getInstance
    (
        void
    );

/**
 * @brief Destructor to clean up resources
 */
    ~CalibrationData();

  private:

    enum CalibrationDataQuery_Type
    {
        PUT_CALIBRATION_INTRINSICS,
        GET_CALIBRATION_INTRINSICS,
        PUT_CALIBRATION_EXTRINSICS,
        GET_CALIBRATION_EXTRINSICS,
        CALIBRATION_DATA_QUERY_TYPE_COUNT
    };

/**
 * @brief Private constructor to initialize the SQLite database connection
 */
    CalibrationData();

/**
 * @brief Delete copy constructor and assignment operator to enforce singleton
 * pattern
 */
    CalibrationData
    (
        const CalibrationData &
    ) = delete;
    CalibrationData &operator=
    (
        const CalibrationData &
    ) = delete;

/**
 * @brief Prepare SQLite statements for various calibration data queries
 */
    bool prepareStatements
    (
        void
    );

/**
 * @brief Initialize the SQLite database connection
 */
    bool initializeConnection
    (
        void
    );

/**
 * @brief Initialize the singleton class instance
 */
    bool initializeDatabase
    (
        void
    );

/**
 * @brief Singleton instance
 */
    static std::shared_ptr<CalibrationData> instance_;

    static std::string dbPath_;
    static std::string sqlSchemaPath_;

/**
 * @brief Array of prepared SQLite statements for different calibration data
 * queries
 */
    std::array<sqlite3_stmt *, CALIBRATION_DATA_QUERY_TYPE_COUNT> preparedStatements_;

/**
 * @brief SQLite database connection
 */
    sqlite3 *db_;

    std::shared_ptr<GSLogger> logger_;
}; // End class CalibrationData
} // End namespace PiTrac

#endif // __CLIBRATION_DATA_H__