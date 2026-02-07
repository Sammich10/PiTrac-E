-- Create Calibration Data Schema Tables

-- Table to store database schema version information, including version number and creation date.
-- This helps in tracking the schema version used in the database. When the database is created by 
-- software, this table is initialized with the current schema version and creation date.
CREATE TABLE IF NOT EXISTS Database_Info(
    SchemaVersion INTEGER PRIMARY KEY,
    DateCreated TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Calibrations (
    CalibrationID INTEGER PRIMARY KEY AUTOINCREMENT,
    CameraID INTEGER NOT NULL,
    CalibrationType TEXT NOT NULL,
    CalibrationDate TEXT NOT NULL,
    FOREIGN KEY (CameraID) REFERENCES Camera_Info(CameraID)
)

CREATE TABLE IF NOT EXISTS Camera_Info (
    CameraID INTEGER PRIMARY KEY AUTOINCREMENT,
    CameraName TEXT NOT NULL,
    CameraType TEXT NOT NULL,
    Manufacturer TEXT,
    ModelNumber TEXT,
    SerialNumber TEXT
);

CREATE TABLE IF NOT EXISTS Distortion_Coefficients (
    CoefficientID INTEGER PRIMARY KEY AUTOINCREMENT,
    CalibrationID INTEGER NOT NULL,
    K1 REAL,
    K2 REAL,
    P1 REAL,
    P2 REAL,
    K3 REAL,
    FOREIGN KEY (CalibrationID) REFERENCES Distortion_Calibration(CalibrationID)
);

CREATE TABLE IF NOT EXISTS Intrinsic_Calibration (
    IntrinsicID INTEGER PRIMARY KEY AUTOINCREMENT,
    CalibrationID INTEGER NOT NULL,
    FX REAL NOT NULL,
    FY REAL NOT NULL,
    CX REAL NOT NULL,
    CY REAL NOT NULL,
    FOREIGN KEY (CalibrationID) REFERENCES Distortion_Calibration(CalibrationID)
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