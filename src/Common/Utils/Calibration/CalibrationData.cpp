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
    {   // Ensure all data is flushed to disk and WAL file is cleared before closing the database connection
        sqlite3_exec(db_, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
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
    {   // Database file does not exist, will be created during connection initialization
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
    {   // Database file does not exist, we expect that the database is created when 
        // we attempt to open a connection, and then the schema will be initialized in initializeDatabase()
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
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_DISTORTION], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_DISTORTION statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Query to retrieve distortion coefficients for a calibration entry
    statement = "SELECT K1, K2, P1, P2, K3 FROM Distortion_Coefficients WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_DISTORTION], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_DISTORTION statement: " + std::string(sqlite3_errmsg(db_)));
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
    // // Query calibration entries for a camera ID, sort by date
    // statement = "SELECT CalibrationID, CalibrationType, created_at, ReprojectionError FROM Calibration_Entries WHERE CameraID = ?1 ORDER BY created_at DESC;";
    // sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_ENTRY], nullptr);
    // if (sql_status != SQLITE_OK)
    // {
    //     logger_->error("Failed to prepare GET_CALIBRATION_ENTRY statement: " + std::string(sqlite3_errmsg(db_)));
    //     return false;
    // }
    // // Query calibration entries for a camera ID, sort by reprojection error
    // statement = "SELECT CalibrationID, CalibrationType, created_at, ReprojectionError FROM Calibration_Entries WHERE CameraID = ?1 ORDER BY ReprojectionError ASC;";
    // sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_ENTRY], nullptr);
    // if (sql_status != SQLITE_OK)
    // {
    //     logger_->error("Failed to prepare GET_CALIBRATION_ENTRY statement: " + std::string(sqlite3_errmsg(db_)));
    //     return false;
    // }

    // Get best calibration entry data for a camera UUID (lowest reprojection error). 
    // This is a monster query that joins the calibration entries with the camera info, 
    // distortion coefficients, and intrinsic parameters to return all relevant data 
    // for the best calibration entry for a given camera UUID
    statement = 
    // Select all relevant data from all tables
    "SELECT CE.CalibrationID, CE.CalibrationType, CE.created_at, CE.ReprojectionError, "
    "DC.K1, DC.K2, DC.P1, DC.P2, DC.K3, IC.FX, IC.FY, IC.CX, IC.CY "
    "FROM Calibration_Entries CE "
    "INNER JOIN Camera_Info CI ON CE.CameraID = CI.CameraID "
    "INNER JOIN Distortion_Coefficients DC ON CE.CalibrationID = DC.CalibrationID "
    "INNER JOIN Intrinsic_Calibration IC ON CE.CalibrationID = IC.CalibrationID "
    "WHERE CI.UUID = ?1 ORDER BY CE.ReprojectionError ASC LIMIT 1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_BEST_CALIBRATION_ENTRY_BY_UUID], nullptr);
    if (sql_status != SQLITE_OK)    {
        logger_->error("Failed to prepare GET_BEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    // Get latest calibration entry data for a camera UUID (most recent by created_at)
    statement = 
    "SELECT CE.CalibrationID, CE.CalibrationType, CE.created_at, CE.ReprojectionError, "
    "DC.K1, DC.K2, DC.P1, DC.P2, DC.K3, IC.FX, IC.FY, IC.CX, IC.CY "
    "FROM Calibration_Entries CE "
    "INNER JOIN Camera_Info CI ON CE.CameraID = CI.CameraID "
    "INNER JOIN Distortion_Coefficients DC ON CE.CalibrationID = DC.CalibrationID "
    "INNER JOIN Intrinsic_Calibration IC ON CE.CalibrationID = IC.CalibrationID "
    "WHERE CI.UUID = ?1 ORDER BY CE.created_at DESC LIMIT 1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_LATEST_CALIBRATION_ENTRY_BY_UUID], nullptr);
    if (sql_status != SQLITE_OK)    {
        logger_->error("Failed to prepare GET_LATEST_CALIBRATION_ENTRY_BY_UUID statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    return true;
}

bool CalibrationData::putCameraInfo(const CameraInfo_Type &cameraInfo)
{
    sqlite3_stmt *stmt = preparedStatements_[PUT_CAMERA_INFO];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, cameraInfo.uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, cameraInfo.camera_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, cameraInfo.uuid.c_str(), -1, SQLITE_TRANSIENT);
    
    // Use retry logic for better concurrency handling
    if (!executeWithRetry(stmt, 5)) // Retry up to 5 times
    {
        logger_->error("Failed to execute PUT_CAMERA_INFO statement after retries: " + std::string(sqlite3_errmsg(db_)));
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

bool CalibrationData::putCalibrationEntry(const std::string &camera_uuid, const CalibrationEntry_Type &entryInfo, const DistortionCoefficients_Type &distortionCoeffs, const CameraIntrinsics_Type &intrinsics)
{
    // This function will need to execute multiple statements in a transaction:
    // 1) Insert into Calibration_Entries to create a new calibration entry and get the generated CalibrationID
    // 2) Insert into Distortion_Coefficients using the generated CalibrationID
    // 3) Insert into Intrinsic_Calibration using the generated CalibrationID
    // We will need to use sqlite3_last_insert_rowid to get the generated CalibrationID after inserting into Calibration_Entries
    // We should also wrap this in a transaction to ensure atomicity
    char *errMsg = nullptr;
    
    // Step 1: Insert into Calibration_Entries
    sqlite3_stmt *stmt = preparedStatements_[PUT_CALIBRATION_ENTRY];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, camera_uuid.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, entryInfo.calibration_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, entryInfo.reprojection_error);
    
    if (!executeWithRetry(stmt, 5))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_ENTRY statement after retries: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback transaction on failure
        return false;
    }
    
    // Get the generated CalibrationID for the new entry
    int64_t calibrationID = sqlite3_last_insert_rowid(db_);
    
    // Step 2: Insert into Distortion_Coefficients
    stmt = preparedStatements_[PUT_CALIBRATION_DISTORTION];
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, calibrationID);
    sqlite3_bind_double(stmt, 2, distortionCoeffs.k1);
    sqlite3_bind_double(stmt, 3, distortionCoeffs.k2);
    sqlite3_bind_double(stmt, 4, distortionCoeffs.p1);
    sqlite3_bind_double(stmt, 5, distortionCoeffs.p2);
    sqlite3_bind_double(stmt, 6, distortionCoeffs.k3);
    if (!executeWithRetry(stmt, 5))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_DISTORTION statement after retries: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback transaction on failure
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
    if (!executeWithRetry(stmt, 5))
    {
        logger_->error("Failed to execute PUT_CALIBRATION_INTRINSICS statement after retries: " + std::string(sqlite3_errmsg(db_)));
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr); // Rollback transaction on failure
        return false;
    }

    // Commit transaction
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->error("Failed to commit transaction for putCalibrationEntry: " + std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
        return false;
    }

    return true;
}

bool CalibrationData::getLatestCalibrationEntry(const std::string &uuid, CalibrationEntry_Type &entryInfo, DistortionCoefficients_Type &distortionCoeffs, CameraIntrinsics_Type &intrinsics)
{
    // This function will execute the GET_LATEST_CALIBRATION_ENTRY_BY_UUID statement which joins all relevant tables to get the latest calibration entry data for a given camera UUID
    sqlite3_stmt *stmt = preparedStatements_[GET_LATEST_CALIBRATION_ENTRY_BY_UUID];
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, uuid.c_str(), -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW)
    {
        // Extract data from the row and populate the output parameters
        entryInfo.calibration_id = sqlite3_column_int64(stmt, 0);
        entryInfo.calibration_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        entryInfo.calibration_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        entryInfo.reprojection_error = sqlite3_column_double(stmt, 3);
        // Distortion coefficients
        distortionCoeffs.k1 = sqlite3_column_double(stmt, 4);
        distortionCoeffs.k2 = sqlite3_column_double(stmt, 5);
        distortionCoeffs.p1 = sqlite3_column_double(stmt, 6);
        distortionCoeffs.p2 = sqlite3_column_double(stmt, 7);
        distortionCoeffs.k3 = sqlite3_column_double(stmt, 8);
        // Intrinsic parameters
        intrinsics.focal_length_x = sqlite3_column_double(stmt, 9);
        intrinsics.focal_length_y = sqlite3_column_double(stmt, 10);
        intrinsics.principal_point_x = sqlite3_column_double(stmt, 11);
        intrinsics.principal_point_y = sqlite3_column_double(stmt, 12);

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

void CalibrationData::configureSQLiteForConcurrency()
{
    // Enable WAL mode for better concurrency
    char *errMsg = nullptr;
    if (sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &errMsg) != SQLITE_OK)
    {
        logger_->warning("Failed to enable WAL mode: " + std::string(errMsg ? errMsg : "unknown error"));
        sqlite3_free(errMsg);
    }
    
    // Set busy timeout to handle locks
    if (sqlite3_busy_timeout(db_, 30000) != SQLITE_OK) // 30 second timeout
    {
        logger_->warning("Failed to set busy timeout");
    }
    
    // Additional concurrency settings
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr); // Faster than FULL, still safe with WAL
    sqlite3_exec(db_, "PRAGMA cache_size=10000;", nullptr, nullptr, nullptr);   // Larger cache
    sqlite3_exec(db_, "PRAGMA temp_store=memory;", nullptr, nullptr, nullptr);  // Store temp tables in memory
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

} // End namespace PiTrac