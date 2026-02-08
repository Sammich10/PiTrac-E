#ifndef __CLIBRATION_DATA_H__
#define __CLIBRATION_DATA_H__

#include <array>
#include <string>
#include <vector>
#include <sqlite3.h>
#include <opencv2/opencv.hpp>
#include "Common/Utils/Logging/GSLogger.h"
#include "Common/Utils/Calibration/CalibrationStruct.h"

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

    /**
     * @brief Creates the calibration database if it does not exist, and
     *initializes
     * the schema. This should be called before any other operations to ensure
     *the database is set up properly.
     * @param error Output parameter to hold error message if creation fails
     * @return True if the database was created or already exists, false if
     *there was an error during creation
     */
    static bool createDatabaseIfNotExists
    (
        std::string &error
    );

    bool putCameraInfo
    (
        const CameraInfo_Type &cameraInfo
    );

    bool getCameraInfo
    (
        const std::string &uuid,
        CameraInfo_Type &cameraInfo
    );

    bool putCalibrationEntry
    (
        const std::string &camera_uuid,
        const CalibrationEntry_Type &entryInfo,
        const DistortionCoefficients_Type &distortionCoeffs,
        const CameraIntrinsics_Type &intrinsics
    );

    bool getLatestCalibrationEntry
    (
        const std::string &uuid,
        CalibrationEntry_Type &entryInfo,
        DistortionCoefficients_Type &distortionCoeffs,
        CameraIntrinsics_Type &intrinsics
    );

    bool getBestCalibrationEntry
    (
        const std::string &uuid,
        CalibrationEntry_Type &entryInfo,
        DistortionCoefficients_Type &distortionCoeffs,
        CameraIntrinsics_Type &intrinsics
    );

  private:

    enum CalibrationDataQuery_Type
    {
        PUT_CALIBRATION_ENTRY,
        GET_CALIBRATION_ENTRY_BY_DATE,
        GET_CALIBRATION_ENTRY_BY_SCORE,
        PUT_CALIBRATION_DISTORTION,
        GET_CALIBRATION_DISTORTION,
        PUT_CALIBRATION_INTRINSICS,
        GET_CALIBRATION_INTRINSICS,
        PUT_CAMERA_INFO,
        GET_CAMERA_INFO,
        GET_BEST_CALIBRATION_ENTRY_BY_UUID,
        GET_LATEST_CALIBRATION_ENTRY_BY_UUID,
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
 * @brief Configure SQLite database for better concurrency handling
 */
    void configureSQLiteForConcurrency
    (
        void
    );

/**
 * @brief Execute a prepared statement with retry logic for handling database
 *locks
 * @param stmt The prepared SQLite statement to execute
 * @param maxRetries Maximum number of retry attempts (default: 5)
 * @return True if successful, false if failed after all retries
 */
    bool executeWithRetry
    (
        sqlite3_stmt *stmt,
        int maxRetries = 5
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