#include "tests/functional/Interfaces/Camera/test_image.h"
#include "Interfaces/Camera/imx296/imx296.h"
#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <memory>
#include <thread>

/**
 * @brief Test function for IMX296 camera interface.
 *
 * This function tests the IMX296 camera interface by performing a series of
 * operations
 * including opening the camera, starting continuous capture, retrieving frames,
 * and stopping capture.
 */

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <camera_index>" << std::endl;
        return 1;
    }
    // Parse camera index from command line argument
    const uint32_t cameraIndex = static_cast<uint32_t>(std::stoi(argv[1]));
    // Instantiate the IMX296 camera interface with specific parameters for
    // testing
    const std::unique_ptr<PiTrac::GSCameraInterface> camera =
        std::make_unique<PiTrac::IMX296Camera>(1456, 1088, 2.8f, PiTrac::TriggerMode::FREE_RUNNING);
    // Test opening the camera
    if (test_open_camera(camera.get(), cameraIndex) != 0)
    {
        // If opening the camera fails, print an error message and return
        // failure code
        std::cout << "Camera open test failed." << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Camera open test succeeded." << std::endl;
    }

    if(test_configure_camera(camera.get()) != 0)
    {
        std::cout << "Camera configuration test failed." << std::endl;
        camera->closeCamera();
        return -1;
    }
    else
    {
        std::cout << "Camera configuration test succeeded." << std::endl;
    }
    camera->setExposureTime(10000000);
    camera->startContinuousCapture();
    std::filesystem::create_directory("/tmp/imx296");

    for(size_t i = 0; i < static_cast<size_t>(PiTrac::StreamType::STREAM_TYPE_MAX); ++i)
    {
        // Test switching streams
        camera->switchStream(static_cast<PiTrac::StreamType>(i));
        std::string fname = "/tmp/imx296/stream_" + std::to_string(i) + ".jpg";
        if(test_camera_capture(camera.get(), fname) != 0)
        {
            std::cout << "Stream switch test failed for stream " << i << std::endl;
        }
    }

    camera->stopContinuousCapture();

    // Close the camera
    camera->closeCamera();
    return 0;
}