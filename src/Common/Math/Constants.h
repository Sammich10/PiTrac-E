#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#define _USE_MATH_DEFINES
#include <cmath>

namespace PiTrac
{
class Constants
{
  public:
    // Mathematical constants
    static constexpr double PI = M_PI;
    static constexpr double TWO_PI = 2.0 * M_PI;
    static constexpr double HALF_PI = M_PI / 2.0;
    static constexpr double RAD_TO_DEG = 180.0 / M_PI;
    static constexpr double DEG_TO_RAD = M_PI / 180.0;
    static constexpr double MICROSECONDS_TO_SECONDS = 1e-6;
}; // Class Constants
} // namespace PiTrac

#endif // __CONSTANTS_H__