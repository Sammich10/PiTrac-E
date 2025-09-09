#include "tests/functional/Interfaces/Camera/test_image.h"
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>

using namespace PiTrac;
int test_open_camera(GSCameraInterface *const &camera, const uint32_t cameraIndex)
{
    int32_t result = 0;
    std::cout << "Testing camera: " << camera->toString() << std::endl;
    const bool openSuccess = camera->openCamera(cameraIndex);
    if (!openSuccess)
    {
        std::cerr << "Failed to open camera at index " << cameraIndex << std::endl;
        result = -1;
    }
    return result;
}

int test_configure_camera(GSCameraInterface *const &camera)
{
    int32_t result = 0;
    std::cout << "Configuring camera: " << camera->toString() << std::endl;
    const bool configureSuccess = camera->initializeCamera();
    if (!configureSuccess)
    {
        std::cerr << "Failed to configure camera." << std::endl;
        result = -1;
    }
    return result;
}

int test_camera_capture(GSCameraInterface *const &camera, const std::string &testImagePath)
{
    int32_t result = 0;
    std::cout << "Testing camera capture: " << camera->toString() << std::endl;
    const cv::Mat frame = camera->captureFrame();
    if (frame.empty())
    {
        std::cerr << "Failed to capture frame from camera." << std::endl;
        result = -1;
    }
    else
    {
        std::cout << "Frame info: " << std::endl;
        std::cout << "Channels: " << frame.channels() << std::endl;
        std::cout << "Type: " << frame.type() << std::endl;
        std::cout << "Size: " << frame.size() << std::endl;
        std::cout << "Depth: " << frame.depth() << std::endl;

        cv::imwrite(testImagePath, frame);
        std::cout << "Captured image saved to: " << testImagePath << std::endl;
    }
    return result;
}
