-- Create Calibration Data Schema Tables

-- Table to store database schema version information, including version number and creation date.
-- This helps in tracking the schema version used in the database. When the database is created by 
-- software, this table is initialized with the current schema version and creation date.
CREATE TABLE IF NOT EXISTS Database_Info(
    SchemaVersion INTEGER PRIMARY KEY,
    DateCreated TEXT NOT NULL
);

-- Table to store intrinsic camera calibration parameters.
-- This includes camera model, focal length, sensor dimensions, principal point coordinates,
CREATE TABLE IF NOT EXISTS Distortion_Calibration (
    CalibrationID INTEGER PRIMARY KEY AUTOINCREMENT,
    CameraModel TEXT NOT NULL,
    FocalLength REAL NOT NULL,
    SensorWidth REAL NOT NULL,
    SensorHeight REAL NOT NULL,
    CalibrationDate TEXT NOT NULL
);

-- Table to store extrinsic camera calibration parameters.
-- This includes rotation and translation vectors that define the camera's position and orientation
-- relative to a known reference frame.
CREATE TABLE IF NOT EXISTS Extrinsic_Calibration (
    CalibrationID INTEGER PRIMARY KEY,
    RotationVector TEXT NOT NULL,
    TranslationVector TEXT NOT NULL,
    FOREIGN KEY (CalibrationID) REFERENCES Distortion_Calibration(CalibrationID)
);