#include "Common/Utils/Detection/BallDetector.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <algorithm>
#include <cmath>

namespace PiTrac
{
//=============================================================================
// ColorBasedDetection Implementation
//=============================================================================

ColorBasedDetection::ColorBasedDetection(cv::Scalar lower, cv::Scalar upper,
                                         float min_r, float max_r)
    : lower_hsv_(lower), upper_hsv_(upper), min_radius_(min_r), max_radius_(max_r)
{
}

std::vector<BallDetection> ColorBasedDetection::detectBalls(const cv::Mat &frame)
{
    std::vector<BallDetection> detections;

    if (frame.empty())
    {
        return detections;
    }

    cv::Mat hsv, mask, blurred;

    // Convert to HSV for better color filtering
    cv::cvtColor(frame, hsv, cv::COLOR_BGR2HSV);

    // Create color mask
    cv::inRange(hsv, lower_hsv_, upper_hsv_, mask);

    // Morphological operations to clean up the mask
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);

    // Gaussian blur for better circle detection
    cv::GaussianBlur(mask, blurred, cv::Size(9, 9), 2, 2);

    // Detect circles using HoughCircles
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, 1,
                     mask.rows / 8, // min distance between circles
                     100, 30,       // Canny thresholds
                     static_cast<int>(min_radius_), static_cast<int>(max_radius_));

    // Convert to BallDetection objects
    for (const auto &circle : circles)
    {
        cv::Point2f center(circle[0], circle[1]);
        float radius = circle[2];

        // Calculate confidence based on mask density around the circle
        cv::Mat roi;
        cv::Rect roi_rect(
            std::max(0, static_cast<int>(center.x - radius)),
            std::max(0, static_cast<int>(center.y - radius)),
            std::min(static_cast<int>(2 * radius), mask.cols - static_cast<int>(center.x - radius)),
            std::min(static_cast<int>(2 * radius), mask.rows - static_cast<int>(center.y - radius))
            );

        float confidence = 0.8f; // Default confidence for color-based detection
        if (roi_rect.width > 0 && roi_rect.height > 0)
        {
            mask(roi_rect).copyTo(roi);
            confidence = static_cast<float>(cv::countNonZero(roi)) / (roi.rows * roi.cols);
        }

        detections.emplace_back(center, radius, confidence);
    }

    return detections;
}

void ColorBasedDetection::configure(const cv::FileStorage &config)
{
    if (config.isOpened())
    {
        config["lower_hsv"] >> lower_hsv_;
        config["upper_hsv"] >> upper_hsv_;
        config["min_radius"] >> min_radius_;
        config["max_radius"] >> max_radius_;
    }
}

void ColorBasedDetection::setColorRange(cv::Scalar lower, cv::Scalar upper)
{
    lower_hsv_ = lower;
    upper_hsv_ = upper;
}

void ColorBasedDetection::setSizeRange(float min_radius, float max_radius)
{
    min_radius_ = min_radius;
    max_radius_ = max_radius;
}

//=============================================================================
// TemplateMatchingDetection Implementation
//=============================================================================

TemplateMatchingDetection::TemplateMatchingDetection(const cv::Mat &ball_template, float threshold)
    : template_(ball_template.clone()), threshold_(threshold)
{
    // Default scale range
    scales_ = {0.5f, 0.7f, 1.0f, 1.3f, 1.6f, 2.0f};
}

std::vector<BallDetection> TemplateMatchingDetection::detectBalls(const cv::Mat &frame)
{
    std::vector<BallDetection> detections;

    if (frame.empty() || template_.empty())
    {
        return detections;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    for (float scale : scales_)
    {
        cv::Mat scaled_template;
        cv::resize(template_, scaled_template, cv::Size(0, 0), scale, scale);

        cv::Mat result;
        cv::matchTemplate(gray, scaled_template, result, cv::TM_CCOEFF_NORMED);

        // Find locations where matching exceeds threshold
        cv::Mat locations;
        cv::threshold(result, locations, threshold_, 1.0, cv::THRESH_BINARY);

        std::vector<cv::Point> match_locations;
        cv::findNonZero(locations, match_locations);

        for (const auto &pt : match_locations)
        {
            cv::Point2f center(pt.x + scaled_template.cols / 2.0f,
                               pt.y + scaled_template.rows / 2.0f);
            float radius = std::max(scaled_template.cols, scaled_template.rows) / 2.0f;
            float confidence = result.at<float>(pt.y, pt.x);

            detections.emplace_back(center, radius, confidence);
        }
    }

    return detections;
}

void TemplateMatchingDetection::configure(const cv::FileStorage &config)
{
    if (config.isOpened())
    {
        config["threshold"] >> threshold_;
        // Could load template path and scales from config
    }
}

void TemplateMatchingDetection::setTemplate(const cv::Mat &ball_template)
{
    template_ = ball_template.clone();
}

void TemplateMatchingDetection::setScales(const std::vector<float> &scales)
{
    scales_ = scales;
}

//=============================================================================
// ContourBasedDetection Implementation
//=============================================================================

ContourBasedDetection::ContourBasedDetection(double min_area, double max_area,
                                             double min_circularity, double min_convexity)
    : min_area_(min_area), max_area_(max_area)
    , min_circularity_(min_circularity), min_convexity_(min_convexity)
{
}

std::vector<BallDetection> ContourBasedDetection::detectBalls(const cv::Mat &frame)
{
    std::vector<BallDetection> detections;

    if (frame.empty())
    {
        return detections;
    }

    cv::Mat gray, binary;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);

    // Apply Gaussian blur and threshold
    cv::GaussianBlur(gray, gray, cv::Size(9, 9), 2);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    // Find contours
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    for (const auto &contour : contours)
    {
        double area = cv::contourArea(contour);

        // Filter by area
        if (area < min_area_ || area > max_area_)
        {
            continue;
        }

        // Calculate circularity
        double perimeter = cv::arcLength(contour, true);
        double circularity = 4 * CV_PI * area / (perimeter * perimeter);

        if (circularity < min_circularity_)
        {
            continue;
        }

        // Calculate convexity
        std::vector<cv::Point> hull;
        cv::convexHull(contour, hull);
        double hull_area = cv::contourArea(hull);
        double convexity = area / hull_area;

        if (convexity < min_convexity_)
        {
            continue;
        }

        // Calculate enclosing circle
        cv::Point2f center;
        float radius;
        cv::minEnclosingCircle(contour, center, radius);

        // Confidence based on how well contour fits circle
        float confidence = static_cast<float>(circularity * convexity);

        detections.emplace_back(center, radius, confidence);
    }

    return detections;
}

void ContourBasedDetection::configure(const cv::FileStorage &config)
{
    if (config.isOpened())
    {
        config["min_area"] >> min_area_;
        config["max_area"] >> max_area_;
        config["min_circularity"] >> min_circularity_;
        config["min_convexity"] >> min_convexity_;
    }
}

//=============================================================================
// BallDetector Implementation
//=============================================================================

BallDetector::BallDetector(std::unique_ptr<IBallDetectionAlgorithm> algorithm,
                           const BallDetectionConfig &config)
    : algorithm_(std::move(algorithm)), config_(config)
{
}

bool BallDetector::processFrame(const cv::Mat &frame)
{
    if (!algorithm_ || frame.empty())
    {
        return false;
    }

    ++total_frames_processed_;

    // Detect balls in current frame
    auto current_detections = algorithm_->detectBalls(frame);

    // Filter detections by confidence and size
    current_detections.erase(
        std::remove_if(current_detections.begin(), current_detections.end(),
                       [this](const BallDetection &detection) {
            return detection.confidence < config_.min_confidence ||
            detection.radius < config_.min_radius ||
            detection.radius > config_.max_radius;
        }),
        current_detections.end());

    total_detections_ += current_detections.size();

    // Call detection callback
    for (const auto &detection : current_detections)
    {
        if (on_ball_detected_)
        {
            on_ball_detected_(detection);
        }
    }

    // Add to recent detections
    recent_detections_.push_back(current_detections);

    // Keep only required number of frames
    while (static_cast<int>(recent_detections_.size()) > config_.required_consecutive_frames)
    {
        recent_detections_.pop_front();
    }

    // Validate detections
    validateDetections();

    // Clean up old detections
    cleanupOldDetections();

    bool has_validated_balls = !validated_balls_.empty();
    ball_detected_flag_.store(has_validated_balls);

    return has_validated_balls;
}

void BallDetector::validateDetections()
{
    validated_balls_.clear();

    if (static_cast<int>(recent_detections_.size()) < config_.required_consecutive_frames)
    {
        return;
    }

    // Check each detection in the most recent frame
    const auto &latest_frame = recent_detections_.back();

    for (const auto &current_detection : latest_frame)
    {
        int consecutive_matches = 1; // Current frame counts as 1

        // Check if this detection appears in previous frames
        for (auto it = recent_detections_.rbegin() + 1; it != recent_detections_.rend(); ++it)
        {
            bool found_match = findMatchingDetection(current_detection, *it, config_.max_movement_distance);

            if (found_match)
            {
                ++consecutive_matches;
            }
            else
            {
                break; // Must be consecutive
            }
        }

        // If we have enough consecutive matches, validate the detection
        if (consecutive_matches >= config_.required_consecutive_frames)
        {
            validated_balls_.push_back(current_detection);
            ++validated_detections_;

            if (on_ball_validated_)
            {
                on_ball_validated_(current_detection, consecutive_matches);
            }
        }
    }
}

bool BallDetector::findMatchingDetection(const BallDetection &current,
                                         const std::vector<BallDetection> &previous,
                                         float max_distance) const
{
    for (const auto &prev_detection : previous)
    {
        float distance = current.distanceTo(prev_detection);
        if (distance <= max_distance)
        {
            return true;
        }
    }
    return false;
}

void BallDetector::cleanupOldDetections()
{
    auto now = std::chrono::high_resolution_clock::now();

    validated_balls_.erase(
        std::remove_if(validated_balls_.begin(), validated_balls_.end(),
                       [this, now](const BallDetection &detection) {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - detection.timestamp);
            return age > config_.max_detection_age;
        }),
        validated_balls_.end());
}

void BallDetector::setAlgorithm(std::unique_ptr<IBallDetectionAlgorithm> algorithm)
{
    algorithm_ = std::move(algorithm);
    reset(); // Clear state when changing algorithm
}

BallDetector::DetectionStats BallDetector::getStats() const
{
    DetectionStats stats{};
    stats.frames_processed = total_frames_processed_;
    stats.total_detections = total_detections_;
    stats.validated_detections = validated_detections_;
    stats.detection_rate = total_frames_processed_ > 0 ?
                           static_cast<float>(total_detections_) / total_frames_processed_ : 0.0f;
    stats.validation_rate = total_detections_ > 0 ?
                            static_cast<float>(validated_detections_) / total_detections_ : 0.0f;

    return stats;
}

void BallDetector::reset()
{
    recent_detections_.clear();
    validated_balls_.clear();
    ball_detected_flag_.store(false);
    total_frames_processed_ = 0;
    total_detections_ = 0;
    validated_detections_ = 0;
}

void BallDetector::drawDebugInfo(cv::Mat &frame, bool show_all_detections) const
{
    if (frame.empty())
    {
        return;
    }

    // Draw validated balls in green
    for (const auto &ball : validated_balls_)
    {
        cv::circle(frame, ball.center, static_cast<int>(ball.radius),
                   cv::Scalar(0, 255, 0), 3);
        cv::circle(frame, ball.center, 2, cv::Scalar(0, 255, 0), -1);

        // Add confidence text
        std::string conf_text = "Conf: " + std::to_string(ball.confidence).substr(0, 4);
        cv::putText(frame, conf_text,
                    cv::Point(static_cast<int>(ball.center.x - ball.radius),
                              static_cast<int>(ball.center.y - ball.radius - 10)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }

    // Optionally draw all recent detections in yellow
    if (show_all_detections && !recent_detections_.empty())
    {
        for (const auto &detection : recent_detections_.back())
        {
            cv::circle(frame, detection.center, static_cast<int>(detection.radius),
                       cv::Scalar(0, 255, 255), 2);
        }
    }

    // Draw statistics
    auto stats = getStats();
    std::vector<std::string> info_text = {
        "Algorithm: " + (algorithm_ ? algorithm_->getAlgorithmName() : "None"),
        "Frames: " + std::to_string(stats.frames_processed),
        "Detections: " + std::to_string(stats.total_detections),
        "Validated: " + std::to_string(stats.validated_detections),
        "Det Rate: " + std::to_string(stats.detection_rate).substr(0, 5),
        "Val Rate: " + std::to_string(stats.validation_rate).substr(0, 5)
    };

    for (size_t i = 0; i < info_text.size(); ++i)
    {
        cv::putText(frame, info_text[i],
                    cv::Point(10, 25 + static_cast<int>(i * 25)),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
    }
}

//=============================================================================
// BallDetectionFactory Implementation
//=============================================================================

std::unique_ptr<IBallDetectionAlgorithm> BallDetectionFactory::createAlgorithm(
    AlgorithmType type, const cv::FileStorage &config)
{
    switch (type)
    {
        case AlgorithmType::COLOR_BASED:
        {
            auto algorithm = std::make_unique<ColorBasedDetection>();
            algorithm->configure(config);
            return algorithm;
        }
        case AlgorithmType::TEMPLATE_MATCHING:
        {
            // Default empty template - must be set later
            cv::Mat default_template = cv::Mat::zeros(20, 20, CV_8UC1);
            auto algorithm = std::make_unique<TemplateMatchingDetection>(default_template);
            algorithm->configure(config);
            return algorithm;
        }
        case AlgorithmType::CONTOUR_BASED:
        {
            auto algorithm = std::make_unique<ContourBasedDetection>();
            algorithm->configure(config);
            return algorithm;
        }
    }
    return nullptr;
}

std::unique_ptr<IBallDetectionAlgorithm> BallDetectionFactory::createFromConfig(
    const std::string &config_file)
{
    cv::FileStorage config(config_file, cv::FileStorage::READ);
    if (!config.isOpened())
    {
        return nullptr;
    }

    std::string algorithm_type;
    config["algorithm_type"] >> algorithm_type;

    if (algorithm_type == "color_based")
    {
        return createAlgorithm(AlgorithmType::COLOR_BASED, config);
    }
    else if (algorithm_type == "template_matching")
    {
        return createAlgorithm(AlgorithmType::TEMPLATE_MATCHING, config);
    }
    else if (algorithm_type == "contour_based")
    {
        return createAlgorithm(AlgorithmType::CONTOUR_BASED, config);
    }

    return nullptr;
}

std::unique_ptr<BallDetector> BallDetectionFactory::createDetector(
    const std::string &config_file)
{
    cv::FileStorage config(config_file, cv::FileStorage::READ);
    if (!config.isOpened())
    {
        return nullptr;
    }

    // Create algorithm
    auto algorithm = createFromConfig(config_file);
    if (!algorithm)
    {
        return nullptr;
    }

    // Load detector configuration
    BallDetectionConfig detector_config;
    config["required_consecutive_frames"] >> detector_config.required_consecutive_frames;
    config["max_movement_distance"] >> detector_config.max_movement_distance;
    config["min_confidence"] >> detector_config.min_confidence;
    config["min_radius"] >> detector_config.min_radius;
    config["max_radius"] >> detector_config.max_radius;

    int max_age_ms;
    config["max_detection_age_ms"] >> max_age_ms;
    detector_config.max_detection_age = std::chrono::milliseconds(max_age_ms);

    return std::make_unique<BallDetector>(std::move(algorithm), detector_config);
}
} // namespace PiTrac