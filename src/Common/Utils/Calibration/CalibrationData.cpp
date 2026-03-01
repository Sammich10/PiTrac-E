#include "Common/Utils/Json/jsonparser.h"
#include "Common/Utils/Json/structdef/CalConfig.h"
#include "Common/Utils/Calibration/CalibrationData.h"
#include "Common/Utils/FileUtils/FileUtils.h"
#include <stdexcept>
#include <thread>
#include <chrono>


namespace PiTrac
{
std::shared_ptr<CalibrationData> CalibrationData::instance_ = nullptr;

// Define static member variables
std::string CalibrationData::dbPath_ = "/var/lib/pitrac/data/calibration_data.db";
std::string CalibrationData::sqlSchemaPath_ = "/var/lib/pitrac/data/CalibrationDataSchema.sql";

std::shared_ptr<CalibrationData> CalibrationData::getInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = std::shared_ptr<CalibrationData>(new CalibrationData());
    }
    return instance_;
}

CalibrationData::CalibrationData()
    : db_(nullptr)
    , preparedStatements_{nullptr}
    , logger_(nullptr)
{
    logger_ = GSLogger::getInstance();
    if (!initializeConnection())
    {
        logger_->error("Failed to initialize connection to calibration database.");
    }
    if (!prepareStatements())
    {
        logger_->error("Failed to prepare SQL statements for calibration database.");
    }
}

CalibrationData::~CalibrationData()
{
    if(db_)
    {   // Use passive checkpoint to avoid blocking other processes
        sqlite3_exec(db_, "PRAGMA wal_checkpoint(PASSIVE);", nullptr, nullptr, nullptr);
    }
    for (auto &stmt : preparedStatements_)
    {
        if (stmt)
        {
            sqlite3_finalize(stmt);
        }
    }
    if (db_)
    {
        sqlite3_close(db_);
    }
}

bool CalibrationData::createDatabaseIfNotExists(std::string &error)
{
    if(!FileUtils::fileExists(sqlSchemaPath_) && !FileUtils::fileExists(dbPath_))
    {   // Schema file and database file do not exist, cannot proceed
        error = "CalibrationData initialization failed: SQL schema file " + sqlSchemaPath_ + " and database file " + dbPath_ + " do not exist";
        return false;
    }
    else if(!FileUtils::fileExists(sqlSchemaPath_))
    {   // Schema file does not exist, cannot proceed
        error = "CalibrationData initialization failed: SQL schema file " + sqlSchemaPath_ + " does not exist";
        return false;
    }
    else if(!FileUtils::fileExists(dbPath_))
    {   // Database file does not exist, will be created during connection
        // initialization
        // Ensure the directory for the database file exists or create it
        if(!FileUtils::directoryExists(FileUtils::getDirectoryFromPath(dbPath_)))
        {
            if(!FileUtils::createDirectory(FileUtils::getDirectoryFromPath(dbPath_)))
            {
                error = "Failed to create directory for calibration database at " + FileUtils::getDirectoryFromPath(dbPath_);
                return false;
            }
        }
        sqlite3 *temp_db;
        // Create the database file by opening a connection
        if (sqlite3_open(dbPath_.c_str(), &temp_db) != SQLITE_OK)
        {
            error = "Failed to create new calibration database at " + dbPath_ + ": " + std::string(sqlite3_errmsg(temp_db));
            return false;
        }
        // Now initialize the database schema using the provided SQL schema file
        // Load and execute SQL schema
        std::string schemaSQL;
        if (!FileUtils::readFileToString(sqlSchemaPath_, schemaSQL))
        {
            error = "Failed to read SQL schema file at " + sqlSchemaPath_;
            return false;
        }
        char *errMsg = nullptr;
        if (sqlite3_exec(temp_db, schemaSQL.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            error = "Failed to execute SQL schema: " + std::string(errMsg);
            sqlite3_free(errMsg);
            return false;
        }
    }

    return true;
}

bool CalibrationData::initializeConnection(void)
{
    if(!FileUtils::fileExists(dbPath_))
    {   // Database file does not exist, we expect that the database is created
        // when
        // we attempt to open a connection, and then the schema will be
        // initialized in initializeDatabase()
        logger_->info("Calibration database file " + dbPath_ + " does not exist");
        return false;
    }
    else
    {
        // Open database
        if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK)
        {
            logger_->error("Failed to open existing calibration database at " + dbPath_ + ": " + std::string(sqlite3_errmsg(db_)));
            return false;
        }

        // Configure SQLite for better concurrency
        configureSQLiteForConcurrency();
    }
    return true;
}

bool CalibrationData::prepareStatements()
{
    // Prepare SQLite statements for calibration data queries here
    int sql_status = SQLITE_OK;
    // Query to insert distortion coefficients for a calibration entry
    std::string statement = "INSERT INTO Distortion_Coefficients (CalibrationID , K1, K2, P1, P2, K3) VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_DISTORTION_STANDARD], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_DISTORTION_STANDARD statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve distortion coefficients for a calibration entry
    statement = "SELECT K1, K2, P1, P2, K3 FROM Distortion_Coefficients WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_DISTORTION_STANDARD], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_DISTORTION_STANDARD statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to insert or update distortion coefficients for a calibration entry
    statement = "INSERT OR REPLACE INTO Camera_Controls (CameraID, ExposureTimeUs, AnalogGain, FOVScale) VALUES (?1, ?2, ?3, ?4);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[SET_CAMERA_SETTINGS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare SET_CAMERA_SETTINGS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve camera settings for a camera UUID
    statement = "SELECT ExposureTimeUs, AnalogGain, FOVScale FROM Camera_Controls WHERE CameraID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CAMERA_SETTINGS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CAMERA_SETTINGS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to insert intrinsic parameters for a calibration entry
    statement = "INSERT INTO Intrinsic_Calibration (CalibrationID , FX, FY, CX, CY) VALUES (?1, ?2, ?3, ?4, ?5);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve intrinsic parameters for a calibration entry
    statement = "SELECT FX, FY, CX, CY FROM Intrinsic_Calibration WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to insert fisheye distortion coefficients for a calibration entry
    statement = "INSERT INTO Fisheye_Distortion_Coefficients (CalibrationID , K1, K2, K3, K4) VALUES (?1, ?2, ?3, ?4, ?5);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_DISTORTION_FISHEYE], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_DISTORTION_FISHEYE statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve fisheye distortion coefficients for a calibration entry
    statement = "SELECT K1, K2, K3, K4 FROM Fisheye_Distortion_Coefficients WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_DISTORTION_FISHEYE], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_DISTORTION_FISHEYE statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve intrinsic parameters for a calibration entry
    statement = "SELECT FX, FY, CX, CY FROM Intrinsic_Calibration WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to insert or update camera info
    statement = "INSERT OR REPLACE INTO Camera_Info (CameraName, CameraType, UUID) VALUES (?1, ?2, ?3);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CAMERA_INFO], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CAMERA_INFO statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve camera info
    statement = "SELECT CameraName, CameraType FROM Camera_Info WHERE UUID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CAMERA_INFO], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CAMERA_INFO statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to insert a new calibration entry
    statement = "INSERT INTO Calibration_Entries (CameraID, CalibrationType, ReprojectionError) VALUES (?1, ?2, ?3);";;
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_ENTRY], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_ENTRY statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Get best calibration entry data for a camera UUID (lowest reprojection
    // error).
    statement =
        // Select
        "SELECT CE.CalibrationID, CE.CalibrationType, CE.created_at, CE.ReprojectionError "
        "FROM Calibration_Entries CE "
        "INNER JOIN Camera_Info CI ON CE.CameraID = CI.UUID "
        "WHERE CI.UUID = ?1 ORDER BY CE.ReprojectionError ASC LIMIT 1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_BEST_CALIBRATION_ENTRY_BY_UUID], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_BEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Get latest calibration entry data for a camera UUID (most recent by
    // created_at)
    statement =
        "SELECT CE.CalibrationID, CE.CalibrationType, CE.created_at, CE.ReprojectionError "
        "FROM Calibration_Entries CE "
        "INNER JOIN Camera_Info CI ON CE.CameraID = CI.UUID "
        "WHERE CI.UUID = ?1 ORDER BY CE.created_at DESC LIMIT 1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_LATEST_CALIBRATION_ENTRY_BY_UUID], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_LATEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Insert extrinsic calibration (rotation and translation vectors)
    statement = "INSERT INTO Extrinsic_Calibration (CalibrationID, RVecX, RVecY, RVecZ, TVecX, TVecY, TVecZ, ReprojectionError, NumPoints) "
                "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_EXTRINSIC_CALIBRATION], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_EXTRINSIC_CALIBRATION statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Get extrinsic calibration by calibration ID
    statement = "SELECT RVecX, RVecY, RVecZ, TVecX, TVecY, TVecZ, ReprojectionError, NumPoints "
                "FROM Extrinsic_Calibration WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_EXTRINSIC_CALIBRATION], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_EXTRINSIC_CALIBRATION statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    return true;
}

bool CalibrationData::putCameraInfo(const CameraInfo_Type &cameraInfo)
{
    sqlite3_stmt *stmt = preparedStatements_[PUT_CAMERA_INFO];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, cameraInfo.camera_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, cameraInfo.camera_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cameraInfo.uuid.c_str(), -1, SQLITE_TRANSIENT);

    // Use retry logic for better concurrency handling
    if (!executeWithRetry(stmt, 5)) // Retry up to 5 times
    {
        logger_->error("Failed to execute PUT_CAMERA_INFO statement after retries: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        return false;
    }
    sqlite3_reset(stmt); // Reset statement after execution for next use
    sqlite3_clear_bindings(stmt); // Clear bindings after execution
    return true;
}

bool CalibrationData::getCameraInfo(const std::string &uuid, CameraInfo_Type &cameraInfo)
{
    sqlite3_stmt *stmt = preparedStatements_[GET_CAMERA_INFO];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        cameraInfo.camera_name = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        cameraInfo.camera_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        return true;
    }
    else if (rc == SQLITE_DONE)
    {
        logger_->warning("No camera info found for UUID: " + uuid);
        return false;
    }
    else
    {
        logger_->error("Failed to execute GET_CAMERA_INFO statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
}

bool CalibrationData::setCameraSettings(const std::string &camera_uuid, const CameraControlSettings_Type &settings)
{
    sqlite3_stmt *stmt = preparedStatements_[SET_CAMERA_SETTINGS];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, camera_uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, settings.exposure_time_us);
    sqlite3_bind_double(stmt, 3, settings.analog_gain);
    sqlite3_bind_double(stmt, 4, settings.fov_scale);

    // Use retry logic for better concurrency handling
    if (!executeWithRetry(stmt, 5)) // Retry up to 5 times
    {
        logger_->error("Failed to execute SET_CAMERA_SETTINGS statement after retries: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        return false;
    }
    sqlite3_reset(stmt); // Reset statement after execution for next use
    sqlite3_clear_bindings(stmt); // Clear bindings after execution
    return true;
}

bool CalibrationData::getCameraSettings(const std::string &camera_uuid, CameraControlSettings_Type &settings)
{
    sqlite3_stmt *stmt = preparedStatements_[GET_CAMERA_SETTINGS];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, camera_uuid.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        settings.exposure_time_us = sqlite3_column_int(stmt, 0);
        settings.analog_gain = static_cast<float>(sqlite3_column_double(stmt, 1));
        settings.fov_scale = static_cast<float>(sqlite3_column_double(stmt, 2));
        return true;
    }
    else if (rc == SQLITE_DONE)
    {
        logger_->warning("No camera settings found for UUID: " + camera_uuid);
        return false;
    }
    else
    {
        logger_->error("Failed to execute GET_CAMERA_SETTINGS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
}

bool CalibrationData::putCalibrationEntry(const std::string &camera_uuid,
                                          const CalibrationEntry_Type &entryInfo,
                                          const DistortionCoefficients_Type &distortionCoeffs,
                                          const CameraIntrinsics_Type &intrinsics)
{
    // This function will need to execute multiple statements in a transaction:
    // 1) Insert into Calibration_Entries to create a new calibration entry and
    // get the generated CalibrationID
    // 2) Insert into Distortion_Coefficients using the generated CalibrationID
    // 3) Insert into Intrinsic_Calibration using the generated CalibrationID
    // We will need to use sqlite3_last_insert_rowid to get the generated
    // CalibrationID after inserting into Calibration_Entries
    // We should also wrap this in a transaction to ensure atomicity
    char *errMsg = nullptr;

    // Use BEGIN IMMEDIATE to get exclusive write access immediately and fail
    // fast if another process has a lock
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->error("Failed to begin immediate transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
        if (errMsg)
        {
            sqlite3_free(errMsg);
        }

        // If immediate transaction fails, wait briefly and try a regular
        // transaction as fallback
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            logger_->error("Failed to begin fallback transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
            if (errMsg)
            {
                sqlite3_free(errMsg);
            }
            return false;
        }
    }

    // Step 1: Insert into Calibration_Entries
    sqlite3_stmt *stmt = preparedStatements_[PUT_CALIBRATION_ENTRY];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, camera_uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, (int)entryInfo.calibration_type);
    sqlite3_bind_double(stmt, 3, entryInfo.reprojection_error);

    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_ENTRY statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Get the generated CalibrationID for the new entry
    int64_t calibrationID = sqlite3_last_insert_rowid(db_);

    // Step 2: Insert into Distortion_Coefficients
    stmt = preparedStatements_[PUT_CALIBRATION_DISTORTION_STANDARD];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, calibrationID);
    sqlite3_bind_double(stmt, 2, distortionCoeffs.k1);
    sqlite3_bind_double(stmt, 3, distortionCoeffs.k2);
    sqlite3_bind_double(stmt, 4, distortionCoeffs.p1);
    sqlite3_bind_double(stmt, 5, distortionCoeffs.p2);
    sqlite3_bind_double(stmt, 6, distortionCoeffs.k3);
    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_DISTORTION_STANDARD statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Step 3: Insert into Intrinsic_Calibration
    stmt = preparedStatements_[PUT_CALIBRATION_INTRINSICS];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, calibrationID);
    sqlite3_bind_double(stmt, 2, intrinsics.focal_length_x);
    sqlite3_bind_double(stmt, 3, intrinsics.focal_length_y);
    sqlite3_bind_double(stmt, 4, intrinsics.principal_point_x);
    sqlite3_bind_double(stmt, 5, intrinsics.principal_point_y);
    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Commit transaction
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->error("Failed to commit transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // on commit
                                                                   // failure
        return false;
    }

    return true;
}

bool CalibrationData::putCalibrationEntry(const std::string &camera_uuid,
                                          const CalibrationEntry_Type &entryInfo,
                                          const FisheyeDistortionCoefficients_Type &distortionCoeffs,
                                          const CameraIntrinsics_Type &intrinsics)
{
    // This function will need to execute multiple statements in a transaction:
    // 1) Insert into Calibration_Entries to create a new calibration entry and
    // get the generated CalibrationID
    // 2) Insert into Distortion_Coefficients using the generated CalibrationID
    // 3) Insert into Intrinsic_Calibration using the generated CalibrationID
    // We will need to use sqlite3_last_insert_rowid to get the generated
    // CalibrationID after inserting into Calibration_Entries
    // We should also wrap this in a transaction to ensure atomicity
    char *errMsg = nullptr;

    // Use BEGIN IMMEDIATE to get exclusive write access immediately and fail
    // fast if another process has a lock
    if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->error("Failed to begin immediate transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
        if (errMsg)
        {
            sqlite3_free(errMsg);
        }

        // If immediate transaction fails, wait briefly and try a regular
        // transaction as fallback
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            logger_->error("Failed to begin fallback transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
            if (errMsg)
            {
                sqlite3_free(errMsg);
            }
            return false;
        }
    }

    // Step 1: Insert into Calibration_Entries
    sqlite3_stmt *stmt = preparedStatements_[PUT_CALIBRATION_ENTRY];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, camera_uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, (int)entryInfo.calibration_type);
    sqlite3_bind_double(stmt, 3, entryInfo.reprojection_error);

    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_ENTRY statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Get the generated CalibrationID for the new entry
    int64_t calibrationID = sqlite3_last_insert_rowid(db_);

    // Step 2: Insert into Distortion_Coefficients
    stmt = preparedStatements_[PUT_CALIBRATION_DISTORTION_FISHEYE];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, calibrationID);
    sqlite3_bind_double(stmt, 2, distortionCoeffs.k1);
    sqlite3_bind_double(stmt, 3, distortionCoeffs.k2);
    sqlite3_bind_double(stmt, 4, distortionCoeffs.k3);
    sqlite3_bind_double(stmt, 5, distortionCoeffs.k4);
    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_DISTORTION_FISHEYE statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Step 3: Insert into Intrinsic_Calibration
    stmt = preparedStatements_[PUT_CALIBRATION_INTRINSICS];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, calibrationID);
    sqlite3_bind_double(stmt, 2, intrinsics.focal_length_x);
    sqlite3_bind_double(stmt, 3, intrinsics.focal_length_y);
    sqlite3_bind_double(stmt, 4, intrinsics.principal_point_x);
    sqlite3_bind_double(stmt, 5, intrinsics.principal_point_y);
    if (!executeWithRetry(stmt, 1))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt); // Reset statement after failure
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // transaction
                                                                   // on failure
        return false;
    }

    // Commit transaction
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->error("Failed to commit transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback
                                                                   // on commit
                                                                   // failure
        return false;
    }

    return true;
}

bool CalibrationData::getLatestCalibrationEntry(const std::string &uuid, CalibrationEntry_Type &entryInfo)
{
    // This function will execute the GET_LATEST_CALIBRATION_ENTRY_BY_UUID
    // statement which joins all relevant tables to get the latest calibration
    // entry data for a given camera UUID
    sqlite3_stmt *stmt = preparedStatements_[GET_LATEST_CALIBRATION_ENTRY_BY_UUID];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        // Extract data from the row and populate the output parameters
        entryInfo.calibration_id = sqlite3_column_int64(stmt, 0);
        entryInfo.calibration_type = static_cast<CalibrationModel>(sqlite3_column_int(stmt, 1));
        entryInfo.calibration_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        entryInfo.reprojection_error = sqlite3_column_double(stmt, 3);
        return true;
    }
    else if (rc == SQLITE_DONE)
    {
        logger_->warning("No calibration entries found for camera UUID: " + uuid);
        return false;
    }
    else
    {
        logger_->error("Failed to execute GET_LATEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    return (rc == SQLITE_ROW);
}

bool CalibrationData::getBestCalibrationEntry(const std::string &uuid, CalibrationEntry_Type &entryInfo)
{
    // This function will execute the GET_BEST_CALIBRATION_ENTRY_BY_UUID
    // statement which joins all relevant tables to get the best calibration
    // entry data for a given camera UUID
    sqlite3_stmt *stmt = preparedStatements_[GET_BEST_CALIBRATION_ENTRY_BY_UUID];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        // Extract data from the row and populate the output parameters
        entryInfo.calibration_id = sqlite3_column_int64(stmt, 0);
        entryInfo.calibration_type = static_cast<CalibrationModel>(sqlite3_column_int(stmt, 1));
        entryInfo.calibration_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        entryInfo.reprojection_error = sqlite3_column_double(stmt, 3);
        return true;
    }
    else if (rc == SQLITE_DONE)
    {
        logger_->warning("No calibration entries found for camera UUID: " + uuid);
        return false;
    }
    else
    {
        logger_->error("Failed to execute GET_BEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
}

bool CalibrationData::getCalibrationEntryData(CalibrationEntry_Type &entryInfo, DistortionCoefficients_Type &distortionCoeffs, CameraIntrinsics_Type &intrinsics)
{
    // This function will execute separate queries to get the distortion
    // coefficients and intrinsic parameters for a given CalibrationID
    sqlite3_stmt *stmt = preparedStatements_[GET_CALIBRATION_DISTORTION_STANDARD];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, entryInfo.calibration_id);
    int rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
    {
        logger_->error("Failed to execute GET_CALIBRATION_DISTORTION_STANDARD statement for CalibrationID " + std::to_string(entryInfo.calibration_id) + ": " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Extract distortion coefficients from the row and populate the output
    // parameter
    distortionCoeffs.k1 = sqlite3_column_double(stmt, 0);
    distortionCoeffs.k2 = sqlite3_column_double(stmt, 1);
    distortionCoeffs.p1 = sqlite3_column_double(stmt, 2);
    distortionCoeffs.p2 = sqlite3_column_double(stmt, 3);
    distortionCoeffs.k3 = sqlite3_column_double(stmt, 4);
    sqlite3_reset(stmt);
    stmt = preparedStatements_[GET_CALIBRATION_INTRINSICS];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, entryInfo.calibration_id);
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
    {
        logger_->error("Failed to execute GET_CALIBRATION_INTRINSICS statement for CalibrationID " + std::to_string(entryInfo.calibration_id) + ": " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Extract intrinsic parameters from the row and populate the output
    // parameter
    intrinsics.focal_length_x = sqlite3_column_double(stmt, 0);
    intrinsics.focal_length_y = sqlite3_column_double(stmt, 1);
    intrinsics.principal_point_x = sqlite3_column_double(stmt, 2);
    intrinsics.principal_point_y = sqlite3_column_double(stmt, 3);
    return true; // Placeholder - implement actual queries and data extraction
}

bool CalibrationData::getCalibrationEntryData(CalibrationEntry_Type &entryInfo, FisheyeDistortionCoefficients_Type &distortionCoeffs, CameraIntrinsics_Type &intrinsics)
{
    // This function will execute separate queries to get the fisheye distortion
    // coefficients and intrinsic parameters for a given CalibrationID
    sqlite3_stmt *stmt = preparedStatements_[GET_CALIBRATION_DISTORTION_FISHEYE];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, entryInfo.calibration_id);
    int rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
    {
        logger_->error("Failed to execute GET_CALIBRATION_DISTORTION_FISHEYE statement for CalibrationID " + std::to_string(entryInfo.calibration_id) + ": " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Extract distortion coefficients from the row and populate the output
    // parameter
    distortionCoeffs.k1 = sqlite3_column_double(stmt, 0);
    distortionCoeffs.k2 = sqlite3_column_double(stmt, 1);
    distortionCoeffs.k3 = sqlite3_column_double(stmt, 2);
    distortionCoeffs.k4 = sqlite3_column_double(stmt, 3);
    sqlite3_reset(stmt);
    stmt = preparedStatements_[GET_CALIBRATION_INTRINSICS];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, entryInfo.calibration_id);
    rc = sqlite3_step(stmt);
    if(rc != SQLITE_ROW)
    {
        logger_->error("Failed to execute GET_CALIBRATION_INTRINSICS statement for CalibrationID " + std::to_string(entryInfo.calibration_id) + ": " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Extract intrinsic parameters from the row and populate the output
    // parameter
    intrinsics.focal_length_x = sqlite3_column_double(stmt, 0);
    intrinsics.focal_length_y = sqlite3_column_double(stmt, 1);
    intrinsics.principal_point_x = sqlite3_column_double(stmt, 2);
    intrinsics.principal_point_y = sqlite3_column_double(stmt, 3);
    return true; // Placeholder - implement actual queries and data extraction
}

void CalibrationData::configureSQLiteForConcurrency()
{
    // Enable WAL mode for better concurrency with retry logic
    char *errMsg = nullptr;
    int attempts = 0;
    const int maxAttempts = 5;
    bool walEnabled = false;

    while (attempts < maxAttempts)
    {
        if (sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg) == SQLITE_OK)
        {
            walEnabled = true;
            logger_->info("WAL mode enabled successfully");
            break; // Success
        }
        else if (errMsg && (strstr(errMsg, "database is locked") || strstr(errMsg, "busy")))
        {
            // Database locked, retry with backoff
            attempts++;
            if (errMsg)
            {
                sqlite3_free(errMsg);
                errMsg = nullptr;
            }

            if (attempts < maxAttempts)
            {
                logger_->warning("Database locked while configuring WAL mode, attempt " +
                                 std::to_string(attempts) + "/" + std::to_string(maxAttempts) + ", retrying...");
                int sleepMs = (1 << (attempts - 1)) * 50; // 50ms, 100ms, 200ms,
                                                          // 400ms
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
        }
        else
        {
            // Other error, don't retry
            logger_->error("Failed to enable WAL mode: " + std::string(errMsg ? errMsg : "unknown error"));
            if (errMsg)
            {
                sqlite3_free(errMsg);
            }
            break;
        }
    }

    if (!walEnabled)
    {
        logger_->error("Failed to enable WAL mode after " + std::to_string(maxAttempts) + " attempts - database will use default journal mode with reduced concurrency");
    }

    // Set busy timeout to handle locks (this rarely fails, but add basic error
    // handling)
    if (sqlite3_busy_timeout(db_, 10000) != SQLITE_OK) // Reduced to 10 second
                                                       // timeout for faster
                                                       // failure
    {
        logger_->warning("Failed to set busy timeout");
    }

    // Additional concurrency settings (these are less likely to fail, but
    // they're fast operations)
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr); // Faster
                                                                                // than
                                                                                // FULL,
                                                                                // still
                                                                                // safe
                                                                                // with
                                                                                // WAL
    sqlite3_exec(db_, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);   // Larger
                                                                                // cache
    sqlite3_exec(db_, "PRAGMA temp_store=memory;", nullptr, nullptr, nullptr);  // Store
                                                                                // temp
                                                                                // tables
                                                                                // in
                                                                                // memory
    sqlite3_exec(db_, "PRAGMA busy_timeout=10000;", nullptr, nullptr, nullptr); // Also
                                                                                // set
                                                                                // via
                                                                                // PRAGMA
                                                                                // for
                                                                                // redundancy
}

bool CalibrationData::executeWithRetry(sqlite3_stmt *stmt, int maxRetries)
{
    for (int attempt = 0; attempt < maxRetries; ++attempt)
    {
        int result = sqlite3_step(stmt);

        if (result == SQLITE_DONE)
        {
            return true;
        }
        else if (result == SQLITE_BUSY || result == SQLITE_LOCKED)
        {
            logger_->warning("Database busy/locked on attempt " + std::to_string(attempt + 1) +
                             "/" + std::to_string(maxRetries) + ", retrying...");

            // Reset statement for retry
            sqlite3_reset(stmt);

            // Wait with exponential backoff
            int sleepMs = (1 << attempt) * 10; // 10ms, 20ms, 40ms, 80ms, etc.
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
        }
        else
        {
            // Other error - don't retry
            logger_->error("Database error: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
    }

    logger_->error("Failed to execute statement after " + std::to_string(maxRetries) + " attempts: database still busy/locked");
    return false;
}

bool CalibrationData::putExtrinsicCalibration(uint32_t calibration_id, const CameraExtrinsics_Type &extrinsics)
{
    sqlite3_stmt *stmt = preparedStatements_[PUT_EXTRINSIC_CALIBRATION];
    sqlite3_reset(stmt);

    sqlite3_bind_int(stmt, 1, calibration_id);
    sqlite3_bind_double(stmt, 2, extrinsics.rvec[0]);
    sqlite3_bind_double(stmt, 3, extrinsics.rvec[1]);
    sqlite3_bind_double(stmt, 4, extrinsics.rvec[2]);
    sqlite3_bind_double(stmt, 5, extrinsics.tvec[0]);
    sqlite3_bind_double(stmt, 6, extrinsics.tvec[1]);
    sqlite3_bind_double(stmt, 7, extrinsics.tvec[2]);
    sqlite3_bind_double(stmt, 8, extrinsics.reprojection_error);
    sqlite3_bind_int(stmt, 9, extrinsics.num_points);

    if (!executeWithRetry(stmt, 5))
    {
        logger_->error("Failed to execute PUT_EXTRINSIC_CALIBRATION statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt);
        return false;
    }

    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);
    logger_->info("Extrinsic calibration saved for CalibrationID: " + std::to_string(calibration_id) +
                  " (RMS error: " + std::to_string(extrinsics.reprojection_error) + " pixels)");
    return true;
}

bool CalibrationData::getExtrinsicCalibration(uint32_t calibration_id, CameraExtrinsics_Type &extrinsics)
{
    sqlite3_stmt *stmt = preparedStatements_[GET_EXTRINSIC_CALIBRATION];
    sqlite3_reset(stmt);
    sqlite3_bind_int(stmt, 1, calibration_id);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        extrinsics.rvec[0] = sqlite3_column_double(stmt, 0);
        extrinsics.rvec[1] = sqlite3_column_double(stmt, 1);
        extrinsics.rvec[2] = sqlite3_column_double(stmt, 2);
        extrinsics.tvec[0] = sqlite3_column_double(stmt, 3);
        extrinsics.tvec[1] = sqlite3_column_double(stmt, 4);
        extrinsics.tvec[2] = sqlite3_column_double(stmt, 5);
        extrinsics.reprojection_error = sqlite3_column_double(stmt, 6);
        extrinsics.num_points = sqlite3_column_int(stmt, 7);

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return true;
    }
    else if (rc == SQLITE_DONE)
    {
        logger_->warning("No extrinsic calibration found for CalibrationID: " + std::to_string(calibration_id));
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return false;
    }
    else
    {
        logger_->error("Failed to execute GET_EXTRINSIC_CALIBRATION statement: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        return false;
    }
}
} // End namespace PiTrac