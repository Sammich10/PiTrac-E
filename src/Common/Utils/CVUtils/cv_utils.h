/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022-2025, Verdant Consultants, LLC.
 */

#ifndef CV_UTILS_H
#define CV_UTILS_H

#include <string_view>
#include <algorithm>
#include <format>

#include <NumCpp.hpp>
#include <NumCpp/Core/Types.hpp>
#include <NumCpp/NdArray.hpp>

#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <opencv4/opencv2/imgproc.hpp>
#include <opencv4/opencv2/highgui.hpp>

#include "Common/Utils/ColorSys/colorsys.h"

/**
 * @namespace PiTrac
 * @brief Contains utility functions for OpenCV operations.
 */

namespace PiTrac
{
struct CvUtils
/**
 * @brief Utility functions for OpenCV operations.
 *
 * This class provides a collection of static helper functions and constants for
 * common
 * OpenCV tasks, including color conversions, geometric calculations, image
 * manipulation,
 * and unit conversions.
 *
 */
{
    static constexpr float kOpenCvHueMax = 180;
    static constexpr float kOpenCvSatMax = 255;
    static constexpr float kOpenCvValMax = 255;

    /**
     * @brief Returns the radius of a circle represented by a cv::Vec3f.
     *
     * The input vector is expected to contain the circle parameters in the
     * format (x, y, radius),
     * where x and y are the coordinates of the circle center, and radius is the
     * circle's radius.
     *
     * @param circle A cv::Vec3f containing the circle parameters (x, y,
     * radius).
     *        - circle[0]: x-coordinate of the center (unused in this function)
     *        - circle[1]: y-coordinate of the center (unused in this function)
     *        - circle[2]: radius of the circle
     * @return int The radius of the circle as an integer.
     */
    static int CircleRadius
    (
        const cv::Vec3f &circle
    );

    /**
     * @brief Computes the integer (x, y) coordinates of the center of a circle
     * from its parameters.
     *
     * @param circle A cv::Vec3f representing the circle, where:
     *        - circle[0]: x-coordinate of the center
     *        - circle[1]: y-coordinate of the center
     *        - circle[2]: radius of the circle (unused in this function)
     * @return cv::Vec2i The integer (x, y) coordinates of the circle's center.
     */
    static cv::Vec2i CircleXY
    (
        const cv::Vec3f &circle
    );

    /**
     * @brief Returns the x-coordinate of the center of a circle represented by
     * a cv::Vec3f.
     *
     * The input vector is expected to contain the circle parameters in the form
     *(x, y, radius).
     *
     * @param circle A cv::Vec3f containing the circle's center coordinates and
     * radius.
     * @return The x-coordinate of the circle's center as an integer.
     */
    static int CircleX
    (
        const cv::Vec3f &circle
    );

    /**
     * @brief Returns the Y-coordinate of the center of a circle represented by
     * a cv::Vec3f.
     *
     * The input vector should contain the circle parameters in the form (x, y,
     * radius).
     *
     * @param circle A cv::Vec3f containing the circle's center coordinates and
     * radius.
     * @return int The Y-coordinate of the circle's center.
     */
    static int CircleY
    (
        const cv::Vec3f &circle
    );

    /**
     * @brief Returns the size of the given image as a 2D vector.
     *
     * This function extracts the width and height of the input OpenCV matrix
     *(image)
     * and returns them as a cv::Vec2i, where the first element is the width
     *(number of columns)
     * and the second element is the height (number of rows).
     *
     * @param img The input cv::Mat image whose size is to be determined.
     * @return cv::Vec2i A vector containing the width and height of the image.
     */
    static cv::Vec2i CvSize
    (
        const cv::Mat &img
    );

    /**
     * @brief Returns the height (number of rows) of the given OpenCV image.
     *
     * @param img Reference to a cv::Mat object representing the image.
     * @return int The height of the image in pixels.
     */
    static int CvHeight
    (
        const cv::Mat &img
    );

    /**
     * @brief Returns the width of the given OpenCV image.
     *
     * This function retrieves the number of columns (width) in the provided
     * cv::Mat image.
     *
     * @param img Reference to the input cv::Mat image.
     * @return int The width (number of columns) of the image.
     */
    static int CvWidth
    (
        const cv::Mat &img
    );

    /**
     * @brief Converts an angle from degrees to radians.
     *
     * @param deg Angle in degrees.
     * @return Angle in radians.
     */
    static double inline DegreesToRadians
    (
        const double &deg
    )
    {
        return ((deg / 180.0) * CV_PI);
    };

    /**
     * @brief Converts an angle from radians to degrees.
     *
     * @param rad Angle in radians.
     * @return Angle in degrees.
     */
    static double inline RadiansToDegrees
    (
        const double &rad
    )
    {
        return ((rad / CV_PI) * 180.0);
    };

    /**
     * @brief Converts an RGB color value to its HSV representation.
     *
     * This function takes a cv::Scalar representing an RGB color (with channels
     * in the order R, G, B)
     * and returns a cv::Scalar containing the corresponding HSV values (with
     * channels in the order H, S, V).
     *
     * @param rgb The input color as a cv::Scalar in RGB format.
     * @return cv::Scalar The converted color in HSV format.
     * @note The input RGB value is expected to be in OpenCV format (BGR order).
     */
    static cv::Scalar ConvertRgbToHsv
    (
        const cv::Scalar &rgb
    );

    /**
     * @brief Converts a color value from HSV color space to RGB color space.
     *
     * This function takes a cv::Scalar representing a color in HSV format and
     * returns
     * a cv::Scalar representing the equivalent color in RGB format.
     *
     * @param hsv The input color in HSV format (hue, saturation, value).
     * @return cv::Scalar The converted color in RGB format (red, green, blue).
     * @note The input HSV value is expected to be in OpenCV format (H, S, V).
     */
    static cv::Scalar ConvertHsvToRgb
    (
        const cv::Scalar &hsv
    );

    /**
     * @brief Calculates the Euclidean distance between two RGB colors.
     *
     * This function computes the color distance between two colors represented
     * as cv::Scalar objects,
     * typically used for RGB values. The distance is calculated in the RGB
     * color space.
     *
     * @param rgb1 The first color as a cv::Scalar (B, G, R).
     * @param rgb2 The second color as a cv::Scalar (B, G, R).
     * @return The Euclidean distance between the two colors.
     */
    static float ColorDistance
    (
        const cv::Scalar &rgb1,
        const cv::Scalar &rgb2
    );


    /**
     * @brief Extracts the RGB color values of a ball from an image based on the
     * provided circle parameters.
     *
     * This function analyzes the specified region in the input image, defined
     * by the given circle,
     * and returns a vector of cv::Scalar objects representing the RGB color(s)
     * detected within the ball area.
     *
     * @param img The input image (cv::Mat) from which to extract the ball's
     * color.
     * @param circle The circle parameters (cv::Vec3f) representing the ball's
     * position and radius in the image.
     * @return std::vector<cv::Scalar> A vector containing the RGB color(s) of
     * the ball.
     * @note  The ball color will be an average of the colors near the middle of
     * the determined ball. The returned color is in RGB form
     */
    static std::vector<cv::Scalar> GetBallColorRgb
    (
        const cv::Mat &img,
        const cv::Vec3f &circle
    );

    /**
     * @brief Generates a mask image centered around an expected ball position.
     *
     * This function creates a mask image of the specified resolution, with a
     * masked area
     * (either circular or square) centered at the expected ball coordinates.
     * The mask can be
     * used for image processing tasks such as isolating regions of interest.
     *
     * @param resolutionX Width of the mask image in pixels.
     * @param resolutionY Height of the mask image in pixels.
     * @param expected_ball_X X-coordinate of the expected ball position (center
     * of mask).
     * @param expected_ball_Y Y-coordinate of the expected ball position (center
     * of mask).
     * @param mask_radius Radius of the mask area (for circle or half-side for
     * square).
     * @param mask_dimensions Output parameter that will contain the bounding
     * rectangle of the mask area.
     * @param use_square If true, creates a square mask; otherwise, creates a
     * circular mask.
     * @return cv::Mat The generated mask image.
     */
    static cv::Mat GetAreaMaskImage
    (
        int resolutionX,
        int resolutionY,
        int expected_ball_X,
        int expected_ball_Y,
        int mask_radius,
        cv::Rect &mask_dimensions,
        bool use_square = false
    );

    /**
     * @brief Converts a length from meters to feet.
     *
     * @param m Length in meters.
     * @return Equivalent length in feet.
     */
    static double MetersToFeet
    (
        const double &m
    );

    /**
     * @brief Converts a length from meters to inches.
     *
     * @param m Length in meters.
     * @return Equivalent length in inches.
     */
    static double MetersToInches
    (
        const double &m
    );

    /**
     * @brief Converts a measurement from inches to meters.
     *
     * @param i The value in inches to be converted.
     * @return The equivalent value in meters.
     */
    static double InchesToMeters
    (
        const double &i
    );

    /**
     * @brief Sets the size of the result_image to match the size of
     * image_to_size.
     *
     * This function resizes or adjusts the dimensions of result_image so that
     * it matches
     * the size (rows and columns) of image_to_size. The contents of
     * result_image may be
     * altered or reallocated to fit the new size.
     *
     * @param image_to_size The reference image whose size will be used as the
     * target size.
     * @param result_image The image to be resized to match image_to_size.
     */
    static void SetMatSize
    (
        const cv::Mat &image_to_size,
        cv::Mat &result_image
    );

    static void BrightnessAndContrastAutoAlgo1
    (
        const cv::Mat &src,
        cv::Mat &dst,
        float clip_hist_percent = 0
    );

    static void BrightnessAndContrastAutoAlgo2
    (
        const cv::Mat &bgr_image,
        cv::Mat &dst
    );

    static void DrawGrayImgHistogram
    (
        const cv::Mat &img,
        const bool ignore_zeros = false
    );

    // Note - if the ball_ROI_rect rectangle is invalid, it will be corrected.
    static cv::Mat GetSubImage
    (
        const cv::Mat &full_image,
        cv::Rect &ball_ROI_rect,
        cv::Point &offset_sub_to_full,
        cv::Point &offset_full_to_sub
    );

    static bool IsUprightRect
    (
        const float &theta
    );

  private:

    static constexpr float kBYTE_MAX = 255.0f;
    static constexpr float kBYTE_MIN = 0.0f;
};
}

#endif
