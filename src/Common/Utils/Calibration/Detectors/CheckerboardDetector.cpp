#include "Common/Utils/Calibration/Detectors/CheckerboardDetector.h"

namespace PiTrac
{
CheckerboardDetector::CheckerboardDetector(const CalibrationModel model)
    : PatternDetector(model)
    , processImageFunc_(nullptr)
{
    calibrationModel_ = model;
    switch(model)
    {
        case CalibrationModel::FISHEYE:
        {
            processImageFunc_ = std::bind(&CheckerboardDetector::processImageFisheye, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            subPixWinSize_ = cv::Size(5, 5);
            break;
        }
        case CalibrationModel::STANDARD:
        default:
        {
            processImageFunc_ = std::bind(&CheckerboardDetector::processImageStandard, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
            subPixWinSize_ = cv::Size(11, 11);
            break;
        }
    }
}

const ImageQuality CheckerboardDetector::processImage(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    if(image.empty())
    {
        logger_->error("Input image is empty, cannot process");
        return ImageQuality::REJECTED;
    }
    if(image.channels() == 3)
    {
        logger_->info("Input image is not grayscale, converting to grayscale for processing");
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
    }
    else if(image.channels() != 1)
    {
        logger_->error("Input image has unsupported number of channels (%d), expected 1 or 3", image.channels());
        return ImageQuality::REJECTED;
    }
    return processImageFunc_(image, debugImage, corners, objectPoints);
}

const ImageQuality CheckerboardDetector::processImageStandard(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    // Initialize debug image
    debugImage = image.clone();
    // Attempt to find chessboard corners using OpenCV's built-in function with adaptive thresholding and normalization
    bool found = cv::findChessboardCorners(
        image,
        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
        corners,
        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
        );
    // Try with gentle blur to reduce noise
    if(!found)
    {
        logger_->info("Initial chessboard detection failed for image, trying with gentle blur...");
        cv::Mat blurred;
        cv::GaussianBlur(image, blurred, cv::Size(3, 3), 0.5);

        found = cv::findChessboardCorners(
            blurred,
            cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
            );
    }
    // Try with CLAHE to improve contrast (only for standard model, as it can hurt fisheye detection)
    if(!found)
    {
        logger_->info("Chessboard detection with blur failed for image, trying with CLAHE...");
        cv::Mat processedImage;
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
        clahe->apply(image, processedImage);

        found = cv::findChessboardCorners
                (
            processedImage,
            cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE | cv::CALIB_CB_FILTER_QUADS
                );
    }

    if(!found)
    {
        logger_->warning("Chessboard detection failed for image after all attempts");
        // Add failure indicator
        cv::putText(debugImage, "NOT FOUND", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        return ImageQuality::REJECTED;
    }

    // Draw corners on debug image
    cv::drawChessboardCorners(
        debugImage,
        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
        corners,
        found
        );

    // Perform some image quality screening to reject images that are unlikely to yield good calibration results, even if
    // corners were detected. This helps ensure we only use high-quality images for calibration and avoid skewing results
    ImageQuality qualityLabel = screenImageQuality(image, debugImage, corners, objectPoints);

    // Refine corner locations for sub-pixel accuracy
    cv::cornerSubPix(
        image,
        corners,
        subPixWinSize_,
        cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01)
        );
    // Generate 3D object points for this chessboard
    for (uint32_t r = 0; r < checkerboardDimensions_[1]; ++r)
    {
        for (uint32_t c = 0; c < checkerboardDimensions_[0]; ++c)
        {
            objectPoints.emplace_back(c, r, 0.0f);
        }
    }
    return qualityLabel;
}

const ImageQuality CheckerboardDetector::processImageFisheye(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    // Initialize debug image
    debugImage = image.clone();
    // Attempt to find chessboard corners using OpenCV's built-in function with adaptive thresholding and normalization
    bool found = cv::findChessboardCorners(
        image,
        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
        corners,
        cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
        );
    if(!found)
    {
        logger_->info("Initial chessboard detection failed for image, trying with gentle blur...");
        // Try with gentle blur to reduce noise
        cv::Mat blurred;
        cv::GaussianBlur(image, blurred, cv::Size(3, 3), 0.5);

        found = cv::findChessboardCorners(
            blurred,
            cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
            corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE
            );
    }
    if(!found)
    {
        logger_->warning("Chessboard detection failed for image after all attempts");
        // Add failure indicator
        cv::putText(debugImage, "NOT FOUND", cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 255), 2);
        return ImageQuality::REJECTED;
    }

    // Draw corners on debug image
    cv::drawChessboardCorners(
        debugImage,
        cv::Size(checkerboardDimensions_[0], checkerboardDimensions_[1]),
        corners,
        found
        );

    // Perform some image quality screening to reject images that are unlikely to yield good calibration results, even if
    // corners were detected. This helps ensure we only use high-quality images for calibration and avoid skewing results
    ImageQuality qualityLabel = screenImageQuality(image, debugImage, corners, objectPoints);

    // Refine corner locations for sub-pixel accuracy
    cv::cornerSubPix(
        image,
        corners,
        subPixWinSize_,
        cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.01)
        );
    // Generate 3D object points for this chessboard
    for (uint32_t r = 0; r < checkerboardDimensions_[1]; ++r)
    {
        for (uint32_t c = 0; c < checkerboardDimensions_[0]; ++c)
        {
            objectPoints.emplace_back(c, r, 0.0f);
        }
    }
    return qualityLabel;
}

const ImageQuality CheckerboardDetector::screenImageQuality(cv::Mat &image, cv::Mat &debugImage, std::vector<cv::Point2f> &corners, std::vector<cv::Point3f> &objectPoints)
{
    // Quality screen 1: Coverage - Ensure corners are well-distributed across the image
    cv::Rect bbox = cv::boundingRect(corners);
    double coverage = (bbox.width * bbox.height) / static_cast<double>(image.cols * image.rows);

    // Quality screen 2: Sharpness - Use Laplacian variance to detect blur
    cv::Mat laplacian;
    cv::Laplacian(image(bbox), laplacian, CV_64F);
    cv::Scalar mean, stddev;
    cv::meanStdDev(laplacian, mean, stddev);
    double sharpness = stddev[0] * stddev[0];

    // Quality screen 3: Corner spread - Compute average distance of corners from their center of mass
    cv::Point2f centerOfMass(0, 0);
    for (const auto &corner : corners)
    {
        centerOfMass += corner;
    }
    centerOfMass *= (1.0f / corners.size());
    double avgDistFromCenter = 0.0;
    for (const auto &corner : corners)
    {
        avgDistFromCenter += cv::norm(corner - centerOfMass);
    }
    avgDistFromCenter /= corners.size();
    double imageDiagonal = std::sqrt(image.cols * image.cols + image.rows * image.rows);
    double normalizedSpread = avgDistFromCenter / imageDiagonal;
    std::vector<std::string> rejectReasons;
    bool accept = true;

    // Coverage screen: ensure that the detected corners cover a reasonable portion of the image (not too small or too large)
    if(coverage < CHECKERBOARD_COVERAGE_TOO_LOW)
    {
        accept = false;
        rejectReasons.push_back("COVERAGE TOO LOW (" + std::to_string(static_cast<int>(coverage * 100)) + "%)");
    }
    else if(coverage > CHECKERBOARD_COVERAGE_TOO_HIGH)
    {
        accept = false;
        rejectReasons.push_back("COVERAGE TOO HIGH (" + std::to_string(static_cast<int>(coverage * 100)) + "%)");
    }
    // Sharpness screen: ensure that the image isn't too blurry (low variance of Laplacian) or unrealistically sharp (high variance, likely noise)
    if(sharpness < SHARPNESS_THRESHOLD_LOW)
    {
        accept = false;
        rejectReasons.push_back("TOO BLURRY (sharpness: " + std::to_string(static_cast<int>(sharpness)) + ")");
    }
    // Spread screen: Ensure the corner points are not too clustered together
    if(normalizedSpread < MIN_CORNER_CLUSTERING)
    {
        accept = false;
        rejectReasons.push_back("CLUSTERED (spread: " + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)");
    }
    // If any quality screen failed, reject the image and log all reasons
    if(!accept)
    {
        logger_->warning("Image rejected: " + std::to_string(rejectReasons.size()) + " quality issues:");
        for(const auto &reason : rejectReasons)
        {
            logger_->warning("  - " + reason);
            cv::putText(debugImage, reason, cv::Point(10, 30 + 30 * rejectReasons.size()), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 165, 255), 2);
        }
        return ImageQuality::REJECTED;
    }
    // Image passed all minimum quality screens - categorize quality based on
    // how well it meets criteria
    ImageQuality qualityLabel;
    cv::Scalar qualityColor;
    if (coverage >= CHECKERBOARD_COVERAGE_GOOD && sharpness > SHARPNESS_THRESHOLD_HIGH && normalizedSpread > CORNER_CLUSTERING_EXCELLENT)
    {
        qualityLabel = ImageQuality::EXCELLENT;
        qualityColor = cv::Scalar(0, 255, 0); // Green
    }
    else if (coverage >= CHECKERBOARD_COVERAGE_TOO_LOW && sharpness > SHARPNESS_THRESHOLD_LOW && normalizedSpread > CORNER_CLUSTERING_GOOD)
    {
        qualityLabel = ImageQuality::VERY_GOOD;
        qualityColor = cv::Scalar(0, 200, 200); // Yellow-green
    }
    else
    {
        qualityLabel = ImageQuality::GOOD;
        qualityColor = cv::Scalar(0, 255, 255); // Yellow
    }
    // Add quality indicator with background box for better readability
    std::string qualityText = imageQualityToString(qualityLabel) + " COVERAGE: (" + std::to_string(static_cast<int>(coverage * 100)) + "%), SHARPNESS: (" +
                              std::to_string(static_cast<int>(sharpness)) + "), SPREAD: (" + std::to_string(static_cast<int>(normalizedSpread * 100)) + "%)";

    // Calculate text size to determine background box dimensions
    int baseline = 0;
    cv::Size textSize = cv::getTextSize(qualityText, cv::FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);

    // Draw semi-transparent black background box
    cv::Point textOrg(10, 30);
    cv::rectangle(debugImage,
                  textOrg + cv::Point(0, baseline),
                  textOrg + cv::Point(textSize.width, -textSize.height),
                  cv::Scalar(0, 0, 0),
                  cv::FILLED);

    // Draw text on top of background
    cv::putText(debugImage, qualityText, textOrg, cv::FONT_HERSHEY_SIMPLEX, 1.0, qualityColor, 2);
    // Add checkerboard info
    cv::putText(debugImage, "Board: " + std::to_string(checkerboardDimensions_[0]) + "x" + std::to_string(checkerboardDimensions_[1]),
                cv::Point(10, 100), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
    return qualityLabel;
}
} // namespace PiTrac