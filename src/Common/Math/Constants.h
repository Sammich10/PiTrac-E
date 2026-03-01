#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#define _USE_MATH_DEFINES
#include <cmath>

namespace PiTrac
{
// Mathematical constants
constexpr double kPi = M_PI;
constexpr double kTwoPi = 2.0 * M_PI;
constexpr double kHalfPi = M_PI / 2.0;
constexpr double kRadToDeg = 180.0 / M_PI;
constexpr double kDegToRad = M_PI / 180.0;
} // namespace PiTrac

#endif // __CONSTANTS_H__