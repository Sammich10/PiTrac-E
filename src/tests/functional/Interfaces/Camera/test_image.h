#include "Interfaces/Camera/GSCameraInterface.h"

using namespace PiTrac;

int test_open_camera
(
    GSCameraInterface *const &camera
);

int test_configure_camera
(
    GSCameraInterface *const &camera
);

int test_camera_capture
(
    GSCameraInterface *const &camera,
    const std::string &testImagePath
);
