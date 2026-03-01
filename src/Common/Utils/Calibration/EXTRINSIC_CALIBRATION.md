# Extrinsic Camera Calibration Guide

## Overview

Extrinsic calibration computes the camera's **position** and **orientation** in 3D world space using known calibration points. This is essential for:

- Determining camera pose relative to a calibration rig
- Converting 2D image coordinates to 3D world coordinates
- Triangulating ball positions from multiple cameras
- Verifying camera alignment and positioning

## Prerequisites

Before performing extrinsic calibration, you **must** complete intrinsic calibration:
1. Lens distortion calibration (fisheye or standard model)
2. Camera matrix calibration (focal lengths and principal point)

These are stored in the database and loaded automatically by the extrinsic calibration system.

## How It Works

### Algorithm: Perspective-n-Point (PnP)

The system uses OpenCV's `cv::solvePnP` to compute camera pose:

**Input:**
- **3D world points**: Known positions of calibration markers (e.g., golf balls on a rig) in real-world coordinates (mm or meters)
- **2D image points**: Detected positions of those same markers in the camera image (pixels)
- **Camera intrinsics**: Pre-calibrated focal lengths, principal point, and distortion coefficients

**Output:**
- **Rotation vector (rvec)**: 3D rotation in Rodrigues format representing camera orientation
- **Translation vector (tvec)**: 3D position of the camera in world coordinates
- **Reprojection error**: RMS error between detected points and back-projected 3D points (quality metric)

### Quality Metrics

| Quality | Reprojection Error |
|---------|-------------------|
| **EXCELLENT** | ≤ 0.5 pixels |
| **GOOD** | ≤ 1.0 pixels |
| **FAIR** | ≤ 2.0 pixels |
| **POOR** | > 2.0 pixels |

## Calibration Rig Setup

### Recommended Approach: Fixed Ball Positions

1. **Create a calibration rig** with golf balls at precisely measured positions
2. **Measure 3D coordinates** of each ball center relative to a world origin
   - Units: millimeters or meters (be consistent)
   - Coordinate system: Define a clear origin (e.g., center of rig)
   - Example: 4 balls in a square pattern, 100mm apart, at 200mm distance from camera

3. **Ball detection requirements:**
   - Minimum: 4 balls (solvePnP requires ≥4 points)
   - Recommended: 6-12 balls for better accuracy
   - Non-coplanar positions improve stability (vary Z-axis positions)

### Example Rig Configuration

```
World Coordinates (mm):
  Origin: Center of rig at camera optical axis

  Ball 1: (-50, -50, 200)   Top-left
  Ball 2: ( 50, -50, 200)   Top-right
  Ball 3: ( 50,  50, 200)   Bottom-right
  Ball 4: (-50,  50, 200)   Bottom-left
```

## Usage

### C++ API

```cpp
#include "Common/Utils/Calibration/CalibrateExtrinsic.h"
#include "Common/Utils/Calibration/CalibrationData.h"

// 1. Load pre-calibrated intrinsics from database
auto calDB = CalibrationData::getInstance();
CalibrationEntry_Type entry;
CameraIntrinsics_Type intrinsics;
FisheyeDistortionCoefficients_Type distortion;

calDB->getLatestCalibrationEntry(cameraUUID, entry);
calDB->getCalibrationEntryData(entry, distortion, intrinsics);

// 2. Create extrinsic calibration object
CalibrateExtrinsic extrinsicCal;
extrinsicCal.setIntrinsics(intrinsics);
extrinsicCal.setDistortion({distortion.k1, distortion.k2, distortion.k3, distortion.k4}, true);

// 3. Add known calibration points (3D world + 2D image)
std::vector<cv::Point3d> worldPoints = {
    cv::Point3d(-50, -50, 200),  // Ball positions on rig (mm)
    cv::Point3d( 50, -50, 200),
    cv::Point3d( 50,  50, 200),
    cv::Point3d(-50,  50, 200)
};

std::vector<cv::Point2d> imagePoints = {
    cv::Point2d(320, 240),  // Detected ball positions in image (pixels)
    cv::Point2d(480, 240),
    cv::Point2d(480, 400),
    cv::Point2d(320, 400)
};

for (size_t i = 0; i < worldPoints.size(); i++)
{
    extrinsicCal.addCalibrationPoint(worldPoints[i], imagePoints[i]);
}

// 4. Perform calibration
auto quality = extrinsicCal.calibrate(true);  // true = use iterative refinement

// 5. Get results
CameraExtrinsics_Type extrinsics = extrinsicCal.getExtrinsics();
std::cout << "Rotation: [" << extrinsics.rvec[0] << ", " 
          << extrinsics.rvec[1] << ", " << extrinsics.rvec[2] << "]" << std::endl;
std::cout << "Translation: [" << extrinsics.tvec[0] << ", " 
          << extrinsics.tvec[1] << ", " << extrinsics.tvec[2] << "]" << std::endl;
std::cout << "Error: " << extrinsics.reprojection_error << " pixels" << std::endl;

// 6. Save to database
calDB->putExtrinsicCalibration(entry.calibration_id, extrinsics);

// 7. Visualize reprojection
cv::Mat debugImage;
extrinsicCal.drawReprojection(debugImage, worldPoints, imagePoints);
cv::imwrite("calibration_result.jpg", debugImage);
```

### Example Program

Run the example calibration program:

```bash
./ExampleExtrinsicCalibration <camera_uuid> <calibration_image.jpg>
```

**Steps:**
1. Place calibration rig in camera view
2. Capture image showing all calibration balls
3. Update `worldPoints` in code with your rig's ball positions
4. Run program - it will detect balls, compute pose, and save results

## Ball Detection

The example uses `cv::HoughCircles` for ball detection. For production use, consider:

1. **Color-based segmentation**: Filter by ball color (white golf balls)
2. **Size filtering**: Known ball diameter in pixels at expected distance
3. **Blob detection**: `cv::SimpleBlobDetector` with circularity constraints
4. **Machine learning**: Trained detector for robust detection under varying lighting

### Ball Detection Tips

- Use consistent lighting
- Ensure balls are clearly visible and not occluded
- High contrast background helps detection
- Mark balls with unique patterns if you need to identify specific balls

## Database Schema

Extrinsic calibration data is stored in the `Extrinsic_Calibration` table:

```sql
CREATE TABLE Extrinsic_Calibration (
    ExtrinsicID INTEGER PRIMARY KEY AUTOINCREMENT,
    CalibrationID INTEGER NOT NULL,
    RVecX REAL NOT NULL,
    RVecY REAL NOT NULL,
    RVecZ REAL NOT NULL,
    TVecX REAL NOT NULL,
    TVecY REAL NOT NULL,
    TVecZ REAL NOT NULL,
    ReprojectionError REAL NOT NULL,
    NumPoints INTEGER NOT NULL,
    FOREIGN KEY (CalibrationID) REFERENCES Calibration_Entries(CalibrationID)
);
```

## Coordinate Systems

### Camera Coordinate System
- **Origin**: Camera optical center
- **Z-axis**: Points along optical axis (into scene)
- **X-axis**: Horizontal (right)
- **Y-axis**: Vertical (down)

### World Coordinate System
- **Origin**: Defined by calibration rig (typically center of rig)
- **Axes**: User-defined (document your convention)

### Rotation Vector (Rodrigues Format)
- Compact 3-element representation of rotation
- Direction: Axis of rotation
- Magnitude: Angle of rotation (radians)
- Convert to rotation matrix: `cv::Rodrigues(rvec, rotationMatrix)`

### Translation Vector
- 3D position of camera in world coordinates
- Units match your world point measurements (mm, meters, etc.)

## Troubleshooting

### High Reprojection Error (> 2 pixels)

**Possible causes:**
- Inaccurate world point measurements
- Poor ball detection (off-center)
- Outdated or incorrect intrinsic calibration
- Lens distortion not properly corrected

**Solutions:**
- Verify rig measurements with calipers
- Improve ball detection algorithm
- Re-run intrinsic calibration
- Use more calibration points (6-12 balls)

### solvePnP Fails to Converge

**Possible causes:**
- Insufficient points (< 4)
- Coplanar points (all balls in same plane)
- Mismatched point correspondence

**Solutions:**
- Add more calibration balls
- Vary ball positions in 3D (different Z depths)
- Verify point ordering matches between worldPoints and imagePoints

### Camera Pose Seems Wrong

**Check:**
1. Coordinate system definition (are axes correct?)
2. Unit consistency (mm vs meters)
3. Point correspondence (Ball 0 in world matches Ball 0 in image?)
4. Intrinsic calibration validity

## Integration with Golf Ball Tracking

Once extrinsic calibration is complete:

1. **Single camera**: Use pose to convert 2D ball position to 3D ray
2. **Dual camera**: Triangulate ball position using both camera poses
3. **Velocity calculation**: Track ball across frames, compute 3D trajectory

For dual-camera setup, you can perform stereo calibration by:
- Calibrating each camera extrinsics relative to shared world origin
- Computing relative pose between cameras (R = R2 * R1^T, T = T2 - R * T1)

## Next Steps

1. Build the calibration library: `make Calibration`
2. Create your calibration rig with measured ball positions
3. Capture calibration image(s)
4. Run extrinsic calibration
5. Verify results with reprojection visualization
6. Integrate extrinsics into your tracking pipeline

## References

- OpenCV solvePnP documentation: https://docs.opencv.org/4.x/d9/d0c/group__calib3d.html#ga549c2075fac14829ff4a58bc931c033d
- Camera calibration theory: https://docs.opencv.org/4.x/dc/dbb/tutorial_py_calibration.html
