#include "Calibration/CalibrateDistortions.h"

namespace pitrac
{
CalibrateDistortions::CalibrateDistortions(cv::Size chessboardSize,
                                           float squareSize,
                                           cv::Size frameSize)
    : chessboardSize(chessboardSize), squareSize(squareSize), frameSize(frameSize)
{
    prepareObjectPoints();
}

void CalibrateDistortions::processImage(const std::string &imagePathPattern)
{
    std::vector<std::string> images;
    cv::glob(imagePathPattern, images);

    for (const auto &imageFile : images)
    {
        std::cout << "Processing Image: " << imageFile << std::endl;
        cv::Mat img = cv::imread(imageFile);
        cv::Mat gray;
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

        std::vector<cv::Point2f> corners;
        bool found = cv::findChessboardCorners(gray, chessboardSize, corners);

        if (found)
        {
            cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                             cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER,
                                              30,
                                              0.001));
            objpoints.push_back(objp);
            imgpoints.push_back(corners);
        }
    }
}

void CalibrateDistortions::calibrateCamera()
{
    cv::calibrateCamera(objpoints, imgpoints, frameSize, cameraMatrix, distCoeffs, rvecs, tvecs);

    // Save calibration results
    cv::FileStorage fs("calibration.yml", cv::FileStorage::WRITE);
    fs << "cameraMatrix" << cameraMatrix;
    fs << "distCoeffs" << distCoeffs;
    fs.release();

    std::cout << "Calibration completed and saved to calibration.yml" << std::endl;
}

void CalibrateDistortions::undistortImage(const std::string &inputImage,
                                          const std::string &outputImage)
{
    cv::Mat img = cv::imread(inputImage);
    cv::Mat undistorted;

    cv::undistort(img, undistorted, cameraMatrix, distCoeffs);
    cv::imwrite(outputImage, undistorted);

    std::cout << "Undistorted image saved to: " << outputImage << std::endl;
}

void CalibrateDistortions::prepareObjectPoints()
{
    objp.resize(chessboardSize.height * chessboardSize.width);
    for (int i = 0; i < chessboardSize.height; ++i)
    {
        for (int j = 0; j < chessboardSize.width; ++j)
        {
            objp[i * chessboardSize.width + j] = cv::Point3f(j * squareSize, i * squareSize, 0);
        }
    }
}

// Usage reference
// int main() {
//     cv::Size chessboardSize(9, 6);
//     float squareSize = 20.0f; // in mm
//     cv::Size frameSize(1456, 1088);

//     CameraCalibration calibrator(chessboardSize, squareSize, frameSize);

//     // Process calibration images
//     calibrator.processImages("./images/cam1/*.png");

//     // Perform calibration
//     calibrator.calibrateCamera();

//     // Undistort a test image
//     calibrator.undistortImage("./test_image_for_undistortion.png",
// "caliResult1.png");

//     return 0;
// }
}

