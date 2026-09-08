# Golf Ball Detection Quick Start Guide

## Quick Integration with CameraAgent

### Option 1: Simple Callback Addition (Easiest)

Add ball detection directly to your existing CameraAgent's `viewfinderCallback()`:

```cpp
// In CameraAgent.h - add member variables
#include "Common/Utils/Detection/BallDetector.h"

private:
    std::unique_ptr<BallDetector> ball_detector_;
    std::atomic<bool> enable_ball_detection_{false};
```

```cpp
// In CameraAgent.cpp constructor - initialize detector
CameraAgent::CameraAgent(...)
{
    // ... existing code ...
    
    // Setup white golf ball detector
    cv::Scalar lower_hsv(0, 0, 200);      // White: any hue, low saturation, bright
    cv::Scalar upper_hsv(180, 50, 255);   
    auto algorithm = std::make_unique<ColorBasedDetection>(lower_hsv, upper_hsv, 5.0f, 100.0f);
    
    BallDetectionConfig config;
    config.required_consecutive_frames = 3;  // 3 frames for validation
    config.min_confidence = 0.5f;
    
    ball_detector_ = std::make_unique<BallDetector>(std::move(algorithm), config);
}
```

```cpp
// In CameraAgent.cpp - modify viewfinderCallback()
void CameraAgent::viewfinderCallback(cv::Mat &frame)
{
    // Add ball detection processing
    if (enable_ball_detection_ && !frame.empty())
    {
        ball_detector_->processFrame(frame);
        ball_detector_->drawDebugInfo(frame, true);  // Draws circles and stats
    }
    
    // Existing code
    frame_buffer_->addFrame(frame);
    if(pause_stream_) return;
    streamFrame(frame, apply_calibrations_to_viewfinder_);
    frame_counter_++;
}
```

### Option 2: Standalone Test (No Agent)

Test ball detection independently:

```cpp
#include "Common/Utils/Detection/BallDetector.h"
#include <opencv2/opencv.hpp>

void testBallDetection()
{
    // Setup detector for white golf balls
    cv::Scalar lower_hsv(0, 0, 200);     
    cv::Scalar upper_hsv(180, 50, 255);  
    PiTrac::ColorBasedDetection detector(lower_hsv, upper_hsv, 5.0f, 100.0f);
    
    // Load or capture frame
    cv::Mat frame = cv::imread("golf_ball_image.jpg");
    
    // Detect
    auto detections = detector.detectBalls(frame);
    
    // Draw results
    for (const auto &ball : detections)
    {
        cv::circle(frame, ball.center, ball.radius, cv::Scalar(0, 255, 0), 3);
        std::cout << "Ball at (" << ball.center.x << ", " << ball.center.y 
                  << ") confidence: " << ball.confidence << std::endl;
    }
    
    cv::imshow("Detections", frame);
    cv::waitKey(0);
}
```

## HSV Color Tuning for White Golf Balls

### Default Settings (Good Starting Point)
```cpp
cv::Scalar lower_hsv(0, 0, 200);      // H: 0-180 (any), S: 0-50 (low), V: 200-255 (bright)
cv::Scalar upper_hsv(180, 50, 255);   
```

### Outdoor/Bright Conditions
```cpp
cv::Scalar lower_hsv(0, 0, 220);      // Higher value threshold
cv::Scalar upper_hsv(180, 40, 255);   // Lower saturation (purer white)
```

### Indoor/Low Light
```cpp
cv::Scalar lower_hsv(0, 0, 150);      // Lower value threshold
cv::Scalar upper_hsv(180, 70, 255);   // Allow more saturation
```

### Green Background (Golf Course)
```cpp
cv::Scalar lower_hsv(0, 0, 200);      
cv::Scalar upper_hsv(180, 50, 255);   
// Works well - white contrasts strongly with green
```

## Detection Parameters

### Size Constraints
```cpp
float min_radius = 5.0f;    // Minimum ball size in pixels
float max_radius = 100.0f;  // Maximum ball size in pixels
```

**Calibration:** Measure ball size at expected distance
- Golf ball diameter = 42.67mm
- If ball at 2m appears as 30 pixels diameter, set max_radius = 40, min_radius = 20

### Validation Settings
```cpp
BallDetectionConfig config;
config.required_consecutive_frames = 3;     // Frames needed for validation
config.max_movement_distance = 50.0f;       // Max pixel movement between frames
config.min_confidence = 0.5f;               // Confidence threshold [0.0-1.0]
config.max_detection_age = std::chrono::milliseconds(1000);
```

**Tuning:**
- **Fast ball:** Increase `max_movement_distance` (e.g., 100.0f)
- **Reduce false positives:** Increase `required_consecutive_frames` (e.g., 5)
- **Noisy detections:** Increase `min_confidence` (e.g., 0.7f)

## Using with Extrinsic Calibration

Integrate detection with your calibration rig:

```cpp
// Detect balls in calibration rig image
ColorBasedDetection detector(lower_hsv, upper_hsv, 5.0f, 100.0f);
auto detections = detector.detectBalls(calibration_image);

// Filter to get best detections
std::vector<cv::Point2d> image_points;
for (const auto &ball : detections)
{
    if (ball.confidence > 0.7f)  // High confidence only
    {
        image_points.push_back(ball.center);
    }
}

// Use with extrinsic calibration
CalibrateExtrinsic extrinsic_cal;
for (size_t i = 0; i < worldPoints.size() && i < image_points.size(); i++)
{
    extrinsic_cal.addCalibrationPoint(worldPoints[i], image_points[i]);
}
extrinsic_cal.calibrate();
```

## Callbacks for Events

```cpp
// Callback for every detection (before validation)
ball_detector_->setDetectionCallback([](const BallDetection &ball) {
    std::cout << "Raw detection at (" << ball.center.x << ", " << ball.center.y << ")" << std::endl;
});

// Callback for validated detections only
ball_detector_->setValidationCallback([](const BallDetection &ball, int frames) {
    std::cout << "VALIDATED ball (seen in " << frames << " frames)" << std::endl;
    // Trigger action: save position, start tracking, etc.
});
```

## Getting Detection Results

```cpp
// Get validated balls
auto validated_balls = ball_detector_->getValidatedBalls();
for (const auto &ball : validated_balls)
{
    std::cout << "Ball at (" << ball.center.x << ", " << ball.center.y 
              << ") radius: " << ball.radius << std::endl;
}

// Check if any ball detected
bool has_ball = ball_detector_->isBallDetected();

// Get statistics
auto stats = ball_detector_->getStats();
std::cout << "Frames: " << stats.frames_processed 
          << " Detections: " << stats.total_detections
          << " Validated: " << stats.validated_detections << std::endl;
```

## Visualization

```cpp
// Draw all debug info (circles, stats text)
ball_detector_->drawDebugInfo(frame, true);  // true = show all detections

// Manual drawing
for (const auto &ball : validated_balls)
{
    // Green circle for validated ball
    cv::circle(frame, ball.center, ball.radius, cv::Scalar(0, 255, 0), 3);
    
    // Add confidence text
    std::string text = "Conf: " + std::to_string(ball.confidence).substr(0, 4);
    cv::putText(frame, text, 
               cv::Point(ball.center.x - ball.radius, ball.center.y - ball.radius - 10),
               cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 2);
}
```

## Troubleshooting

### No Detections

1. **Check HSV range:** Use HSV color picker on your golf ball image
2. **Verify size range:** Print actual ball size in pixels, adjust min/max radius
3. **Test with single frame:** Use `ColorBasedDetection::detectBalls()` directly
4. **Visualize mask:** 
   ```cpp
   cv::Mat hsv, mask;
   cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);
   cv::inRange(hsv, lower_hsv, upper_hsv, mask);
   cv::imshow("Mask", mask);  // White = ball pixels
   ```

### Too Many False Positives

1. **Increase confidence:** `config.min_confidence = 0.7f`
2. **Stricter HSV range:** Narrow the saturation/value bounds
3. **More validation frames:** `config.required_consecutive_frames = 5`
4. **Tighter size constraints:** Reduce min/max radius range

### Ball Not Validated

1. **Check movement limit:** Ball might move >50 pixels between frames
2. **Reduce required frames:** `config.required_consecutive_frames = 2`
3. **Check frame rate:** Ensure consistent frame delivery

## Complete Working Example

See `BallDetectionExample.cpp` for:
- **CameraAgentWithBallDetection** - Extended agent class
- **simpleImageExample()** - Single image test
- **simpleCameraExample()** - Live camera feed test

Build and run:
```bash
cd build
cmake ..
make
./bin/BallDetectionExample
```
