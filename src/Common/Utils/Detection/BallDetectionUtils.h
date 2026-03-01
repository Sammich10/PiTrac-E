#ifndef BALL_DETECTION_UTILS_H
#define BALL_DETECTION_UTILS_H

#include <opencv2/opencv.hpp>
#include <cmath>

namespace PiTrac
{
namespace BallDetectionUtils
{
/**
 * @brief Calculate expected ball radius in pixels based on camera parameters
 *
 * Uses pinhole camera model: pixel_size = (object_size * focal_length) / distance
 *
 * @param ball_diameter_mm Physical diameter of the ball in millimeters (e.g., 42.67mm for golf ball)
 * @param distance_mm Distance from camera to ball in millimeters
 * @param focal_length_px Focal length in pixels (from camera intrinsics: fx or fy)
 * @return Expected ball radius in pixels
 */
inline float calculateExpectedBallRadius(float ball_diameter_mm,
                                         float distance_mm,
                                         float focal_length_px)
{
    // Pinhole camera projection: pixel_diameter = (object_diameter * focal_length) / distance
    float pixel_diameter = (ball_diameter_mm * focal_length_px) / distance_mm;
    return pixel_diameter / 2.0f;     // Return radius
}

/**
 * @brief Calculate expected ball radius range with margin for detection
 *
 * @param ball_diameter_mm Physical diameter of the ball (42.67mm for golf ball)
 * @param min_distance_mm Minimum expected distance to ball
 * @param max_distance_mm Maximum expected distance to ball
 * @param focal_length_px Focal length in pixels (fx or fy from camera matrix)
 * @param margin_factor Safety margin factor (e.g., 1.5 = 50% margin)
 * @param min_radius Output minimum radius for detection
 * @param max_radius Output maximum radius for detection
 */
inline void calculateBallRadiusRange(float ball_diameter_mm,
                                     float min_distance_mm,
                                     float max_distance_mm,
                                     float focal_length_px,
                                     float margin_factor,
                                     float &min_radius,
                                     float &max_radius)
{
    // Ball appears largest when closest
    float max_expected = calculateExpectedBallRadius(ball_diameter_mm, min_distance_mm, focal_length_px);

    // Ball appears smallest when farthest
    float min_expected = calculateExpectedBallRadius(ball_diameter_mm, max_distance_mm, focal_length_px);

    // Apply margin
    max_radius = max_expected * margin_factor;
    min_radius = min_expected / margin_factor;

    // Ensure minimum radius is at least a few pixels
    min_radius = std::max(min_radius, 2.0f);
}

/**
 * @brief Calculate distance to ball given detected pixel radius
 *
 * Inverse of projection: distance = (object_diameter * focal_length) / pixel_diameter
 *
 * @param detected_radius_px Detected ball radius in pixels
 * @param ball_diameter_mm Physical ball diameter (42.67mm for golf ball)
 * @param focal_length_px Focal length in pixels
 * @return Estimated distance to ball in millimeters
 */
inline float calculateBallDistance(float detected_radius_px,
                                   float ball_diameter_mm,
                                   float focal_length_px)
{
    float pixel_diameter = detected_radius_px * 2.0f;
    return (ball_diameter_mm * focal_length_px) / pixel_diameter;
}

/**
 * @brief Get focal length in pixels from camera intrinsics
 *
 * @param camera_matrix 3x3 camera matrix [fx, 0, cx; 0, fy, cy; 0, 0, 1]
 * @param use_horizontal True to use fx (horizontal), false to use fy (vertical)
 * @return Focal length in pixels
 */
inline float getFocalLengthPixels(const cv::Mat &camera_matrix, bool use_horizontal = true)
{
    if (camera_matrix.empty() || camera_matrix.rows != 3 || camera_matrix.cols != 3)
    {
        return 0.0f;
    }

    return use_horizontal ?
           static_cast<float>(camera_matrix.at<double>(0, 0)) :      // fx
           static_cast<float>(camera_matrix.at<double>(1, 1));       // fy
}

/**
 * @brief Calculate focal length in pixels from physical parameters
 *
 * focal_length_px = (focal_length_mm * image_width_px) / sensor_width_mm
 *
 * @param focal_length_mm Physical focal length in millimeters
 * @param image_width_px Image width in pixels
 * @param sensor_width_mm Sensor width in millimeters
 * @return Focal length in pixels
 */
inline float calculateFocalLengthPixels(float focal_length_mm,
                                        int image_width_px,
                                        float sensor_width_mm)
{
    return (focal_length_mm * image_width_px) / sensor_width_mm;
}

/**
 * @brief Golf ball constants
 */
struct GolfBallConstants
{
    static constexpr float DIAMETER_MM = 42.67f;      // Standard golf ball diameter
    static constexpr float RADIUS_MM = 21.335f;       // Standard golf ball radius
    static constexpr float DIAMETER_INCHES = 1.68f;
};

/**
 * @brief Example usage structure for reference
 */
struct BallSizeExample
{
    /**
     * @brief Example: Calculate ball size for IMX219 camera
     *
     * Camera specs:
     * - Resolution: 1456x1088
     * - Sensor: 3.674mm x 2.760mm
     * - Focal length: 2.8mm (fisheye)
     *
     * Golf ball at distance range: 500mm to 3000mm
     */
    static void calculateForIMX219(float &min_radius, float &max_radius)
    {
        // Calculate focal length in pixels
        float focal_length_px = calculateFocalLengthPixels(2.8f, 1456, 3.674f);

        // Expected distance range
        float min_distance_mm = 500.0f;       // 50cm
        float max_distance_mm = 3000.0f;      // 3m

        // Calculate with 50% margin
        calculateBallRadiusRange(
            GolfBallConstants::DIAMETER_MM,
            min_distance_mm,
            max_distance_mm,
            focal_length_px,
            1.5f,      // 50% margin
            min_radius,
            max_radius
            );
    }
};
} // namespace BallDetectionUtils
} // namespace PiTrac

#endif // BALL_DETECTION_UTILS_H
