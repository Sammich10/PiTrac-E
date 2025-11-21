#ifndef __CALIBRATE_DISTORTION_H__
#define __CALIBRATE_DISTORTION_H__

#include <opencv2/opencv.hpp>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "Common/Utils/Logging/GSLogger.h"

namespace PiTrac
{
/**
 * @brief Class containing methods for camera distortion calibration
 */
class CalibrateDistortion
{
  public:


  private:

/**
 * @brief Checkerboard dimensions (number of inner corners per chessboard row
 * and column)
 */
    std::array<uint32_t, 2> checkerboardDimensions_;

    uint32_t numCalibrationImages_;
}; // End class CalibrateDistortion
} // End namespace PiTrac

#endif // __CALIBRATE_DISTORTION_H__