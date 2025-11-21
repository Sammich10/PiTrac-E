#ifndef BALLDETECTOR_H
#define BALLDETECTOR_H

#include <opencv2/opencv.hpp>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <deque>
#include <atomic>

namespace PiTrac
{
/**
 * @brief Represents a detected ball with position, confidence, and metadata
 */
struct BallDetection
{
    cv::Point2f center;           // Ball center position
    float radius;                 // Ball radius in pixels
    float confidence;             // Detection confidence [0.0 - 1.0]
    std::chrono::high_resolution_clock::time_point timestamp;

    BallDetection() = default;
    BallDetection(cv::Point2f c, float r, float conf = 1.0f)
        : center(c), radius(r), confidence(conf)
        , timestamp(std::chrono::high_resolution_clock::now())
    {
    }

    // Calculate distance between two detections
    float distanceTo(const BallDetection &other) const
    {
        cv::Point2f diff = center - other.center;
        return std::sqrt(diff.x * diff.x + diff.y * diff.y);
    }
};

/**
 * @brief Configuration for ball detection validation
 */
struct BallDetectionConfig
{
    // Frame validation settings
    int required_consecutive_frames = 3;     // Frames needed for confirmed
                                             // detection
    float max_movement_distance = 50.0f;    // Max pixels ball can move between
                                            // frames
    float min_confidence = 0.5f;            // Minimum confidence threshold

    // Ball size constraints
    float min_radius = 5.0f;
    float max_radius = 100.0f;

    // Tracking settings
    std::chrono::milliseconds max_detection_age{1000}; // Max time to keep
                                                       // detection

    BallDetectionConfig() = default;
};

/**
 * @brief Abstract base class for ball detection algorithms
 */
class IBallDetectionAlgorithm
{
  public:
    virtual ~IBallDetectionAlgorithm() = default;

    /**
     * @brief Detect balls in a frame
     * @param frame Input frame to process
     * @return Vector of detected balls
     */
    virtual std::vector<BallDetection> detectBalls
    (
        const cv::Mat &frame
    ) = 0;

    /**
     * @brief Get algorithm name for debugging
     */
    virtual std::string getAlgorithmName() const = 0;

    /**
     * @brief Configure algorithm parameters
     */
    virtual void configure(const cv::FileStorage &config)
    {
    }
};

/**
 * @brief Color-based ball detection algorithm
 */
class ColorBasedDetection : public IBallDetectionAlgorithm
{
  private:
    cv::Scalar lower_hsv_;
    cv::Scalar upper_hsv_;
    float min_radius_;
    float max_radius_;

  public:
    ColorBasedDetection
    (
        cv::Scalar lower = cv::Scalar(25, 50, 50),
        cv::Scalar upper = cv::Scalar(35, 255, 255),
        float min_r = 10.0f,
        float max_r = 100.0f
    );

    std::vector<BallDetection> detectBalls
    (
        const cv::Mat &frame
    ) override;
    std::string getAlgorithmName() const override
    {
        return "ColorBased";
    }

    void configure
    (
        const cv::FileStorage &config
    ) override;

    void setColorRange
    (
        cv::Scalar lower,
        cv::Scalar upper
    );
    void setSizeRange
    (
        float min_radius,
        float max_radius
    );
};

/**
 * @brief Template matching ball detection algorithm
 */
class TemplateMatchingDetection : public IBallDetectionAlgorithm
{
  private:
    cv::Mat template_;
    float threshold_;
    std::vector<float> scales_;

  public:
    TemplateMatchingDetection
    (
        const cv::Mat &ball_template,
        float threshold = 0.8f
    );

    std::vector<BallDetection> detectBalls
    (
        const cv::Mat &frame
    ) override;
    std::string getAlgorithmName() const override
    {
        return "TemplateMatching";
    }

    void configure
    (
        const cv::FileStorage &config
    ) override;

    void setTemplate
    (
        const cv::Mat &ball_template
    );
    void setScales
    (
        const std::vector<float> &scales
    );
};

/**
 * @brief Contour-based ball detection algorithm
 */
class ContourBasedDetection : public IBallDetectionAlgorithm
{
  private:
    double min_area_;
    double max_area_;
    double min_circularity_;
    double min_convexity_;

  public:
    ContourBasedDetection
    (
        double min_area = 100.0,
        double max_area = 5000.0,
        double min_circularity = 0.7,
        double min_convexity = 0.8
    );

    std::vector<BallDetection> detectBalls
    (
        const cv::Mat &frame
    ) override;
    std::string getAlgorithmName() const override
    {
        return "ContourBased";
    }

    void configure
    (
        const cv::FileStorage &config
    ) override;
};

/**
 * @brief Main ball detector class with frame validation and event handling
 */
class BallDetector
{
  public:
    // Event callback types
    using DetectionCallback = std::function<void (const BallDetection &)>;
    using ValidationCallback = std::function<void (const BallDetection &, int frame_count)>;

  private:
    std::unique_ptr<IBallDetectionAlgorithm> algorithm_;
    BallDetectionConfig config_;

    // Frame validation tracking
    std::deque<std::vector<BallDetection> > recent_detections_;
    std::vector<BallDetection> validated_balls_;

    // Event handling
    DetectionCallback on_ball_detected_;
    ValidationCallback on_ball_validated_;
    std::atomic<bool> ball_detected_flag_{false};

    // Statistics
    size_t total_frames_processed_ = 0;
    size_t total_detections_ = 0;
    size_t validated_detections_ = 0;

  public:
    /**
     * @brief Constructor with algorithm and configuration
     */
    BallDetector
    (
        std::unique_ptr<IBallDetectionAlgorithm> algorithm,
        const BallDetectionConfig &config = BallDetectionConfig{}
    );

    /**
     * @brief Process a new frame and update detection state
     * @param frame Input frame to process
     * @return true if a validated ball is detected
     */
    bool processFrame
    (
        const cv::Mat &frame
    );

    /**
     * @brief Check if ball detection flag is set
     */
    bool isBallDetected() const
    {
        return ball_detected_flag_.load();
    }

    /**
     * @brief Clear the ball detection flag
     */
    void clearDetectionFlag()
    {
        ball_detected_flag_.store(false);
    }

    /**
     * @brief Get current validated ball detections
     */
    const std::vector<BallDetection> &getValidatedBalls() const
    {
        return validated_balls_;
    }

    /**
     * @brief Set detection callback (called on each frame detection)
     */
    void setOnBallDetected(DetectionCallback callback)
    {
        on_ball_detected_ = callback;
    }

    /**
     * @brief Set validation callback (called when ball is validated)
     */
    void setOnBallValidated(ValidationCallback callback)
    {
        on_ball_validated_ = callback;
    }

    /**
     * @brief Change detection algorithm
     */
    void setAlgorithm
    (
        std::unique_ptr<IBallDetectionAlgorithm> algorithm
    );

    /**
     * @brief Update configuration
     */
    void configure(const BallDetectionConfig &config)
    {
        config_ = config;
    }

    /**
     * @brief Get detection statistics
     */
    struct DetectionStats
    {
        size_t frames_processed;
        size_t total_detections;
        size_t validated_detections;
        float detection_rate;
        float validation_rate;
    };
    DetectionStats getStats() const;

    /**
     * @brief Reset internal state and statistics
     */
    void reset();

    /**
     * @brief Draw debug visualization on frame
     */
    void drawDebugInfo
    (
        cv::Mat &frame,
        bool show_all_detections = false
    ) const;

  private:
    /**
     * @brief Validate detections across multiple frames
     */
    void validateDetections();

    /**
     * @brief Find matching detection in previous frames
     */
    bool findMatchingDetection
    (
        const BallDetection &current,
        const std::vector<BallDetection> &previous,
        float max_distance
    ) const;

    /**
     * @brief Clean up old detections
     */
    void cleanupOldDetections();
};

/**
 * @brief Factory class for creating detection algorithms
 */
class BallDetectionFactory
{
  public:
    enum class AlgorithmType
    {
        COLOR_BASED,
        TEMPLATE_MATCHING,
        CONTOUR_BASED
    };

    /**
     * @brief Create detection algorithm from type
     */
    static std::unique_ptr<IBallDetectionAlgorithm> createAlgorithm
    (
        AlgorithmType type,
        const cv::FileStorage &config = cv::FileStorage{}
    );

    /**
     * @brief Create detection algorithm from configuration file
     */
    static std::unique_ptr<IBallDetectionAlgorithm> createFromConfig
    (
        const std :    : string &config_file
    );

    /**
     * @brief Create complete ball detector from configuration
     */
    static std::unique_ptr<BallDetector> createDetector
    (
        const std::string &config_file
    );
};
} // namespace PiTrac

#endif // BALLDETECTOR_H