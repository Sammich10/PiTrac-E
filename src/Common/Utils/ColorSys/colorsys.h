
/* Copyright 2005-2011 Mark Dufour and contributors; License Expat (See LICENSE)
 */
/* Copyright (C) 2022-2025, Verdant Consultants, LLC. */

#ifndef COLOR_SYS_H
#define COLOR_SYS_H

#include <opencv4/opencv2/core/matx.hpp>
#include <opencv4/opencv2/core.hpp>

namespace PiTrac
{
struct colorsys
/**
 * @class ColorSys
 * @brief Utility class for color space conversions.
 *
 * Provides static methods to convert colors between various color spaces,
 * including RGB, YIQ, HLS, and HSV. All conversions use OpenCV's cv::Scalar
 * to represent color values.
 *
 * Public Methods:
 * - rgb_to_yiq: Converts an RGB color to YIQ color space.
 * - yiq_to_rgb: Converts a YIQ color to RGB color space.
 * - rgb_to_hls: Converts an RGB color to HLS color space.
 * - hls_to_rgb: Converts an HLS color to RGB color space.
 * - rgb_to_hsv: Converts an RGB color to HSV color space.
 * - hsv_to_rgb: Converts an HSV color to RGB color space.
 *
 * Private Members:
 * - Mathematical constants for color conversion calculations.
 * - _v: Helper function for HLS to RGB conversion.
 * - fmods: Computes a non-negative floating-point modulus.
 */
{
  public:
    static cv::Scalar rgb_to_yiq
    (
        const cv::Scalar &rgb
    );
    static cv::Scalar yiq_to_rgb
    (
        const cv::Scalar &yiq
    );
    static cv::Scalar rgb_to_hls
    (
        const cv::Scalar &rgb
    );
    static cv::Scalar hls_to_rgb
    (
        const cv::Scalar &hls
    );
    static cv::Scalar rgb_to_hsv
    (
        const cv::Scalar &rgb
    );
    static cv::Scalar hsv_to_rgb
    (
        const cv::Scalar &hsv
    );

  private:
    static constexpr float ONE_THIRD = (1.0f / 3.0f);
    static constexpr float ONE_SIXTH = (1.0f / 6.0f);
    static constexpr float TWO_THIRD = (2.0f / 3.0f);

    static float _v
    (
        float m1,
        float m2,
        float hue
    );

    /**
     * @brief Computes the floating-point modulus of two numbers, ensuring a
     * non-negative result.'
     *
     * @param a The dividend (numerator).
     * @param b The divisor (denominator).
     * @return The floating-point modulus of `a` and `b`, adjusted to be
     * non-negative with respect to `b`.
     */
    inline static float fmods(float a, float b)
    {
        float f = fmod(a, b);
        if ((f < 0 && b > 0) || (f > 0 && b < 0))
        {
            f += b;
        }
        return f;
    }
};
}

#endif // COLOR_SYS_H