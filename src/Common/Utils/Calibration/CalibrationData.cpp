#include "Common/Utils/Json/jsonparser.h"
#include "Common/Utils/Json/structdef/CalConfig.h"
#include "Common/Utils/Calibration/CalibrationData.h"
#include "Common/Utils/FileUtils/FileUtils.h"

namespace PiTrac
{
std::shared_ptr<CalibrationData> CalibrationData::instance_ = nullptr;

// Define static member variables
std::string CalibrationData::dbPath_ = "calibration_data.db";
std::string CalibrationData::sqlSchemaPath_ = "schemas/CalibrationSchema.sql";

std::shared_ptr<CalibrationData> CalibrationData::getInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = std::shared_ptr<CalibrationData>(new CalibrationData());
        if (!instance_->initializeDatabase())
        {
            instance_ = nullptr;
            throw std::runtime_error("Failed to initialize CalibrationData database.");
        }
    }
    return instance_;
}

CalibrationData::CalibrationData()
    : db_(nullptr)
    , preparedStatements_{nullptr}
    , logger_(nullptr)
{
    logger_ = GSLogger::getInstance();

    const CalConfig calConfig = JsonParser::parseCalConfig("configs/CalConfig.json");
    if (!calConfig.DatabaseFile.empty())
    {
        dbPath_ = calConfig.DatabaseFile;
    }
    if (!calConfig.SchemaFile.empty())
    {
        sqlSchemaPath_ = calConfig.SchemaFile;
    }

    if(!initializeDatabase())
    {
        throw std::runtime_error("Failed to initialize CalibrationData database.");
    }
}

CalibrationData::~CalibrationData()
{
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

bool CalibrationData::initializeDatabase(void)
{
    if (!initializeConnection())
    {
        return false;
    }
    if (!prepareStatements())
    {
        return false;
    }
    return true;
}

bool CalibrationData::initializeConnection(void)
{
    if(!FileUtils::fileExists(sqlSchemaPath_) && !FileUtils::fileExists(dbPath_))
    {   // Schema file and database file do not exist, cannot proceed
        return false;
    }

    if(!FileUtils::fileExists(dbPath_))
    {   // Database file does not exist, create it and set up schema using the
        // schema file
        if(!FileUtils::directoryExists(FileUtils::getDirectoryFromPath(dbPath_)))
        {
            if(!FileUtils::createDirectory(FileUtils::getDirectoryFromPath(dbPath_)))
            {
                return false;
            }
        }

        if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK)
        {
            return false;
        }
        // Load and execute SQL schema
        std::string schemaSQL;
        if (!FileUtils::readFileToString(sqlSchemaPath_, schemaSQL))
        {
            return false;
        }
        char *errMsg = nullptr;
        if (sqlite3_exec(db_, schemaSQL.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            logger_->error("Failed to execute SQL schema: " + std::string(errMsg));
            sqlite3_free(errMsg);
            return false;
        }
    }
    else
    {
        // Open existing database
        if (sqlite3_open(dbPath_.c_str(), &db_) != SQLITE_OK)
        {
            return false;
        }
    }
    return true;
}

bool CalibrationData::prepareStatements()
{
    // Prepare SQLite statements for calibration data queries here
    int sql_status = SQLITE_OK;
    
    std::string statement = "INSERT INTO Distortion_Coefficients (CalibrationID , K1, K2, P1, P2, K3) VALUES (?1, ?2, ?3, ?4, ?5, ?6);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    statement = "SELECT K1, K2, P1, P2, K3 FROM Distortion_Coefficients WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_DISTORTION], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_DISTORTION statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    statement = "INSERT INTO Intrinsics (CalibrationID , FX, FY, CX, CY) VALUES (?1, ?2, ?3, ?4, ?5);";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[PUT_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare PUT_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    statement = "SELECT FX, FY, CX, CY FROM Intrinsics WHERE CalibrationID = ?1;";
    sql_status = sqlite3_prepare_v2(db_, statement.c_str(), -1, &preparedStatements_[GET_CALIBRATION_INTRINSICS], nullptr);
    if (sql_status != SQLITE_OK)
    {
        logger_->error("Failed to prepare GET_CALIBRATION_INTRINSICS statement: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }

    return true;
}
}; // End namespace PiTrac