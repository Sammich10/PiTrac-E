#include <gtest/gtest.h>
#include "Common/Utils/ColorSys/colorsys.h"

namespace PiTrac
{
class ColorsysTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Setup code if needed
    }

    void TearDown() override
    {
        // Cleanup code if needed
    }

    // Helper function for floating point comparison
    bool CompareScalar(const cv::Scalar &a, const cv::Scalar &b, double precision = 1e-2)
    {
        double epsilon = precision;
        return (std::abs(a[0] - b[0]) < epsilon &&
                std::abs(a[1] - b[1]) < epsilon &&
                std::abs(a[2] - b[2]) < epsilon);
    }
}; // class ColorsysTest

// Test RGB to HSV conversion
TEST_F(ColorsysTest, RgbToHsvConversion) {
    cv::Scalar rgb(0.2, 0.4, 0.4);
    cv::Scalar expected_hsv(0.5, 0.5, 0.4);

    cv::Scalar result = colorsys::rgb_to_hsv(rgb);

    EXPECT_TRUE(CompareScalar(result, expected_hsv, 1e-3))
        << "Expected: (" << expected_hsv[0] << ", " << expected_hsv[1] << ", " << expected_hsv[2] <<
        ")"
        << " Got: (" << result[0] << ", " << result[1] << ", " << result[2] << ")";
}

// Test HSV to RGB conversion
TEST_F(ColorsysTest, HsvToRgbConversion) {
    cv::Scalar hsv(0.5, 0.5, 0.4);
    cv::Scalar expected_rgb(0.2, 0.4, 0.4);

    cv::Scalar result = colorsys::hsv_to_rgb(hsv);

    EXPECT_TRUE(CompareScalar(result, expected_rgb, 1e-3))
        << "Expected: (" << expected_rgb[0] << ", " << expected_rgb[1] << ", " << expected_rgb[2] <<
        ")"
        << " Got: (" << result[0] << ", " << result[1] << ", " << result[2] << ")";
}

// Test HLS to RGB conversion
TEST_F(ColorsysTest, HlsToRgbConversion) {
    cv::Scalar hls(1.0, 0.5, 0.7);
    cv::Scalar expected_rgb(0.85, 0.15, 0.15);

    cv::Scalar result = colorsys::hls_to_rgb(hls);

    EXPECT_TRUE(CompareScalar(result, expected_rgb, 1e-3))
        << "Expected: (" << expected_rgb[0] << ", " << expected_rgb[1] << ", " << expected_rgb[2] <<
        ")"
        << " Got: (" << result[0] << ", " << result[1] << ", " << result[2] << ")";
}

// Test RGB to HLS conversion
TEST_F(ColorsysTest, RgbToHlsConversion) {
    cv::Scalar rgb(1.0, 0.5, 0.7);
    cv::Scalar expected_hls(0.93, 0.75, 1.00);

    cv::Scalar result = colorsys::rgb_to_hls(rgb);

    EXPECT_TRUE(CompareScalar(result, expected_hls, 1e-2))
        << "Expected: (" << expected_hls[0] << ", " << expected_hls[1] << ", " << expected_hls[2] <<
        ")"
        << " Got: (" << result[0] << ", " << result[1] << ", " << result[2] << ")";
}

// Test RGB to YIQ conversion
TEST_F(ColorsysTest, RgbToYiqConversion) {
    cv::Scalar rgb(1.0, 0.5, 0.7);
    cv::Scalar expected_yiq(0.67, 0.24, 0.17);

    cv::Scalar result = colorsys::rgb_to_yiq(rgb);

    EXPECT_TRUE(CompareScalar(result, expected_yiq, 1e-2))
        << "Expected: (" << expected_yiq[0] << ", " << expected_yiq[1] << ", " << expected_yiq[2] <<
        ")"
        << " Got: (" << result[0] << ", " << result[1] << ", " << result[2] << ")";
}

// Test round-trip conversion (RGB -> HSV -> RGB)
TEST_F(ColorsysTest, RgbHsvRoundTrip) {
    cv::Scalar original_rgb(0.3, 0.6, 0.9);

    cv::Scalar hsv = colorsys::rgb_to_hsv(original_rgb);
    cv::Scalar result_rgb = colorsys::hsv_to_rgb(hsv);

    EXPECT_TRUE(CompareScalar(original_rgb, result_rgb, 1e-3))
        << "Round-trip conversion failed"
        << " Original: (" << original_rgb[0] << ", " << original_rgb[1] << ", " <<
        original_rgb[2] << ")"
        << " Result: (" << result_rgb[0] << ", " << result_rgb[1] << ", " << result_rgb[2] << ")";
}

// Test edge cases
TEST_F(ColorsysTest, EdgeCases) {
    // Test pure black
    cv::Scalar black(0.0, 0.0, 0.0);
    cv::Scalar hsv_black = colorsys::rgb_to_hsv(black);
    EXPECT_EQ(hsv_black[2], 0.0); // Value should be 0

    // Test pure white
    cv::Scalar white(1.0, 1.0, 1.0);
    cv::Scalar hsv_white = colorsys::rgb_to_hsv(white);
    EXPECT_EQ(hsv_white[1], 0.0); // Saturation should be 0
    EXPECT_EQ(hsv_white[2], 1.0); // Value should be 1
}
} // namespace PiTrac