#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include <NumCpp.hpp>
#include <NumCpp/Core/Types.hpp>
#include <NumCpp/NdArray.hpp>
#include "Common/Utils/CVUtils/cv_utils.h"

namespace PiTrac
{
class CvUtilsTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create test images of different sizes
        test_image_small = cv::Mat::zeros(100, 200, CV_8UC3);  // 200x100
        test_image_large = cv::Mat::ones(480, 640, CV_8UC1);   // 640x480

        // Create test circle
        test_circle = cv::Vec3f(150.0f, 100.0f, 25.0f);  // x=150, y=100,
                                                         // radius=25

        // Create test RGB and HSV colors
        test_rgb = cv::Scalar(128, 64, 192);  // Some purple color
        test_hsv = cv::Scalar(90, 180, 64);   // Some green color
    }

    cv::Mat test_image_small;
    cv::Mat test_image_large;
    cv::Vec3f test_circle;
    cv::Scalar test_rgb;
    cv::Scalar test_hsv;
};

// Test CircleRadius function
TEST_F(CvUtilsTest, CircleRadius) {
    int radius = CvUtils::CircleRadius(test_circle);
    EXPECT_EQ(radius, 25);

    // Test with different radius
    cv::Vec3f circle_large1(100.0f, 50.0f, 75.5f);
    int radius_large1 = CvUtils::CircleRadius(circle_large1);
    EXPECT_EQ(radius_large1, 76);  // Should be rounded to nearest int

    cv::Vec3f circle_large2(100.0f, 50.0f, 75.45f);
    int radius_large2 = CvUtils::CircleRadius(circle_large2);
    EXPECT_EQ(radius_large2, 75);  // Should be rounded to nearest int
}

// Test CircleXY function
TEST_F(CvUtilsTest, CircleXY) {
    cv::Vec2i xy = CvUtils::CircleXY(test_circle);
    EXPECT_EQ(xy[0], 150);
    EXPECT_EQ(xy[1], 100);
}

// Test CircleX function
TEST_F(CvUtilsTest, CircleX) {
    int x = CvUtils::CircleX(test_circle);
    EXPECT_EQ(x, 150);
}

// Test CircleY function
TEST_F(CvUtilsTest, CircleY) {
    int y = CvUtils::CircleY(test_circle);
    EXPECT_EQ(y, 100);
}

// Test CvSize function
TEST_F(CvUtilsTest, CvSize) {
    cv::Vec2i size_small = CvUtils::CvSize(test_image_small);
    EXPECT_EQ(size_small[0], 200);  // width
    EXPECT_EQ(size_small[1], 100);  // height

    cv::Vec2i size_large = CvUtils::CvSize(test_image_large);
    EXPECT_EQ(size_large[0], 640);  // width
    EXPECT_EQ(size_large[1], 480);  // height
}

// Test CvHeight function
TEST_F(CvUtilsTest, CvHeight) {
    int height_small = CvUtils::CvHeight(test_image_small);
    EXPECT_EQ(height_small, 100);

    int height_large = CvUtils::CvHeight(test_image_large);
    EXPECT_EQ(height_large, 480);
}

// Test CvWidth function
TEST_F(CvUtilsTest, CvWidth) {
    int width_small = CvUtils::CvWidth(test_image_small);
    EXPECT_EQ(width_small, 200);

    int width_large = CvUtils::CvWidth(test_image_large);
    EXPECT_EQ(width_large, 640);
}

// Test ConvertRgbToHsv and ConvertHsvToRgb functions
TEST_F(CvUtilsTest, ConvertRgbToHsvAndBack) {
    // Typical RGB value
    cv::Scalar rgb(0.48, 0.64, 0.75); // BGR order for OpenCV
    cv::Scalar hsv = CvUtils::ConvertRgbToHsv(rgb);

    // HSV values should be within OpenCV ranges
    EXPECT_GE(hsv[0], 0);
    EXPECT_LE(hsv[0], CvUtils::kOpenCvHueMax);
    EXPECT_GE(hsv[1], 0);
    EXPECT_LE(hsv[1], CvUtils::kOpenCvSatMax);
    EXPECT_GE(hsv[2], 0);
    EXPECT_LE(hsv[2], CvUtils::kOpenCvValMax);

    // Convert back to RGB and check approximate equality
    cv::Scalar rgb_back = CvUtils::ConvertHsvToRgb(hsv);
    // Allow small rounding error
    EXPECT_NEAR(rgb_back[0], rgb[0], 2);
    EXPECT_NEAR(rgb_back[1], rgb[1], 2);
    EXPECT_NEAR(rgb_back[2], rgb[2], 2);

    // Test with pure colors
    cv::Scalar rgb_red(0, 0, 1.0);
    cv::Scalar hsv_red = CvUtils::ConvertRgbToHsv(rgb_red);
    cv::Scalar rgb_red_back = CvUtils::ConvertHsvToRgb(hsv_red);
    EXPECT_NEAR(rgb_red_back[2], 1.0, 2);

    cv::Scalar rgb_green(0, 1.0, 0);
    cv::Scalar hsv_green = CvUtils::ConvertRgbToHsv(rgb_green);
    cv::Scalar rgb_green_back = CvUtils::ConvertHsvToRgb(hsv_green);
    EXPECT_NEAR(rgb_green_back[1], 1.0, 2);

    cv::Scalar rgb_blue(1.0, 0, 0);
    cv::Scalar hsv_blue = CvUtils::ConvertRgbToHsv(rgb_blue);
    cv::Scalar rgb_blue_back = CvUtils::ConvertHsvToRgb(hsv_blue);
    EXPECT_NEAR(rgb_blue_back[0], 1.0, 2);
}

// Test ColorDistance function
TEST_F(CvUtilsTest, ColorDistance) {
    cv::Scalar rgb1(100, 150, 200);
    cv::Scalar rgb2(100, 150, 200);
    float dist_same = CvUtils::ColorDistance(rgb1, rgb2);
    EXPECT_FLOAT_EQ(dist_same, 0.0f);

    cv::Scalar rgb3(200, 150, 100);
    float dist_diff = CvUtils::ColorDistance(rgb1, rgb3);
    // sqrt((100-200)^2 + (150-150)^2 + (200-100)^2) = sqrt(10000 + 0 + 10000) =
    // sqrt(20000)
    EXPECT_NEAR(dist_diff, std::sqrt(20000.0f), 1e-3);

    cv::Scalar rgb4(0, 0, 0);
    cv::Scalar rgb5(255, 255, 255);
    float dist_max = CvUtils::ColorDistance(rgb4, rgb5);
    EXPECT_NEAR(dist_max, std::sqrt(3 * 255.0f * 255.0f), 1e-3);
}

// Test MetersToFeet function
TEST_F(CvUtilsTest, MetersToFeet) {
    EXPECT_DOUBLE_EQ(CvUtils::MetersToFeet(0.0), 0.0);
    EXPECT_DOUBLE_EQ(CvUtils::MetersToFeet(1.0), 3.281);
    EXPECT_DOUBLE_EQ(CvUtils::MetersToFeet(2.5), 2.5 * 3.281);
}

// Test MetersToInches function
TEST_F(CvUtilsTest, MetersToInches) {
    EXPECT_DOUBLE_EQ(CvUtils::MetersToInches(0.0), 0.0);
    EXPECT_DOUBLE_EQ(CvUtils::MetersToInches(1.0), 12 * 3.281);
    EXPECT_DOUBLE_EQ(CvUtils::MetersToInches(2.5), 12 * 2.5 * 3.281);
}

// Test InchesToMeters function
TEST_F(CvUtilsTest, InchesToMeters) {
    EXPECT_DOUBLE_EQ(CvUtils::InchesToMeters(0.0), 0.0);
    EXPECT_DOUBLE_EQ(CvUtils::InchesToMeters(1.0), 0.0254);
    EXPECT_DOUBLE_EQ(CvUtils::InchesToMeters(10.0), 0.254);
}
}  // namespace PiTrac
