-- Create Calibration Data Schema Tables

-- Table to store database schema version information, including version number and creation date.
-- This helps in tracking the schema version used in the database. When the database is created by 
-- software, this table is initialized with the current schema version and creation date.
CREATE TABLE IF NOT EXISTS Database_Info(
    SchemaVersion INTEGER PRIMARY KEY,
    DateCreated TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS Camera_Info (
    UUID TEXT PRIMARY KEY,
    CameraName TEXT NOT NULL,
    CameraType TEXT NOT NULL,
    unique(UUID)
);

CREATE TABLE IF NOT EXISTS Calibration_Entries (
    CalibrationID INTEGER PRIMARY KEY AUTOINCREMENT,
    CameraID INTEGER NOT NULL,
    CalibrationType INTEGER NOT NULL,
    ReprojectionError REAL NOT NULL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (CameraID) REFERENCES Camera_Info(UUID)
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

CREATE TABLE IF NOT EXISTS Fisheye_Distortion_Coefficients (
    CoefficientID INTEGER PRIMARY KEY AUTOINCREMENT,
    CalibrationID INTEGER NOT NULL,
    K1 REAL,
    K2 REAL,
    K3 REAL,
    K4 REAL,
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

CREATE TABLE IF NOT EXISTS Camera_Controls (
    ControlID INTEGER PRIMARY KEY AUTOINCREMENT,
    CameraID INTEGER NOT NULL,
    ExposureTimeUs INTEGER,
    AnalogGain REAL,
    FOVScale REAL,
    created_at TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at TEXT NOT NULL DEFAULT (datetime('now')),
    FOREIGN KEY (CameraID) REFERENCES Camera_Info(UUID),
    UNIQUE(CameraID)
);

CREATE TABLE IF NOT EXISTS Extrinsic_Calibration (
    ExtrinsicID INTEGER PRIMARY KEY AUTOINCREMENT,
    CalibrationID INTEGER NOT NULL,
    -- Rotation vector (Rodrigues format, 3 values)
    RVecX REAL NOT NULL,
    RVecY REAL NOT NULL,
    RVecZ REAL NOT NULL,
    -- Translation vector (3 values, in world coordinate units)
    TVecX REAL NOT NULL,
    TVecY REAL NOT NULL,
    TVecZ REAL NOT NULL,
    -- Reprojection error for this extrinsic calibration
    ReprojectionError REAL NOT NULL,
    -- Number of calibration points used
    NumPoints INTEGER NOT NULL,
    FOREIGN KEY (CalibrationID) REFERENCES Calibration_Entries(CalibrationID)
);