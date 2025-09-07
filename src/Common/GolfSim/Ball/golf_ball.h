/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022-2025, Verdant Consultants, LLC.
 */


/**
 * \brief The GolfBall class represents a golf ball in the simulation. It
 *contains properties and methods
 * to manage the ball's position, color, and other attributes.
 */

#ifndef GOLF_BALL_H
#define GOLF_BALL_H

#include <map>
#include <numbers>
#include <string>
#include <opencv4/opencv2/core.hpp>
#include <opencv4/opencv2/core/matx.hpp>

#include "Common/GolfSim/Global/gs_globals.h"

namespace PiTrac
{
// Ball constants
// drag coefficient
// https://www.scirp.org/journal/paperinformation.aspx?paperid=85529

// Might as well use some standardized defintion in openCV for this constant (?)
constexpr double kPi = CV_PI;

// These would all be static constants but because some are set from an external
// class, they
// are not.  They may be set in the .JSON file for the system.
//
// Coefficient of drag for an average golf ball
const double kBallDrag_Cd = 0.2;
// Ball mass (kg)
const double kBallMassKg = 0.04592623;

/* TBD - Not currently used
 * // cross-sectional area (m^2)
 * const double kBallCrossSectionAreaM2 = pow(kPi * kBallRadiusMeters, 2);
 * // Ball moment of inertia
 * const double kBallMomentOfInertial_kI = pow(0.4 * kBallMassKg *
 * kBallRadiusMeters, 2);
 * // Magnus coefficient (kg)
 * const double kMagnusCoefficient_S = pow(8 / 3 * kPi * kBallRadiusMeters, 3);
 * // Friction Coefficient with ground
 * const double kFrictionCoefficientWithGround_uK = 0.4;
 * // Using convention: direction right handed golfer is facing = +x, ball
 * flight direction = +y, up = +z
 */

struct BallColorRange
{
    cv::Scalar min;
    cv::Scalar max;
    cv::Scalar center;  // TBD - will we use?
};


class GolfBall
{
  public:
    // Ball radius (m)
    static double kBallRadiusMeters;

  public:

    /**
     * \brief Default constructor for the GolfBall class.
     */
    GolfBall();

    /**
     * \brief Destructor for the GolfBall class.
     */
    ~GolfBall();

    /*
     * TODO: In the long term, need to figure out how to manage x, y, radius,
     * and the ball circle so that
     * we are not carrying redundant information.  We couuld probably derive the
     * circle from the others.
     * center of the ball
     */

    /**
     * \brief Get the center of the ball as a cv::Point2f.
     *
     * @return A cv::Point2f representing the center of the ball in pixels.
     */
    cv::Point2f center() const
    {
        return cv::Point2f(x_, y_);
    }

    /**
     * \brief Return the x-coordinate of the ball's center.
     *
     * @return The x-coordinate in pixels in the OpenCV coordinate system.
     */
    long x() const
    {
        return x_;
    };

    /**
     * \brief Return the y-coordinate of the ball's center.
     *
     * @return The y-coordinate in pixels in the OpenCV coordinate system.
     */
    long y() const
    {
        return y_;
    };

    void set_x(long x)
    {
        x_ = x; ball_circle_[0] = (float)x;
    };
    void set_y(long y)
    {
        y_ = y; ball_circle_[1] = (float)y;
    };

    // Allows type-safe calls that use ball data to set ball data
    void set_x(float x)
    {
        x_ = (long) x; ball_circle_[0] = x;
    };
    void set_y(float y)
    {
        y_ = (long) y; ball_circle_[1] = y;
    };

    // The circle where the ball exists on the relevant image.  Could (in
    // theory?) be different from the
    // more definitive x and y and radius_at_calibration_pixels_!
    cv::Vec3f ball_circle_;

    void set_circle(const cv::Vec3f &c);

    // An ellipse is a more accurate way of representing the ball and is
    // preferred.
    // For example, the ellipse has both an X radius and a Y radius
    // TBD - This is only partially implemented right now.  The hope is that
    // proper camera
    // de-distortion and calibration will make all balls appear circular.
    cv::RotatedRect ball_ellipse_;

    // These next few variables hold information relative to this ball's
    // relationship to another ball.  Typically, the
    // other ball is the same physical ball, but earlier in time.
    cv::Vec3d position_deltas_ball_perspective_;   // Important - the ball
                                                   // movement in the real world
                                                   // between this ball and the
                                                   // prior
    cv::Vec3d distance_deltas_camera_perspective_;
    cv::Vec2d angles_ball_perspective_;
    cv::Vec3d ball_rotation_angles_camera_ortho_perspective_;

    // The distances and angles of this ball from the camera's image center
    cv::Vec3d distances_ortho_camera_perspective_;
    cv::Vec2d angles_camera_ortho_perspective_;

    double measured_radius_pixels_ = 0.0;  // In pixels.  This is the
                                           // currently-known radius, which
                                           // might differ from
                                           // the radius that was measured when
                                           // the ball was originally
                                           // calibrated.
                                           // This is the radius that should
                                           // normally be used to locate the
                                           // ball

    double distance_to_z_plane_from_lens_ = -1;       // Current distance in
                                                      // meters

    // TBD - We've moved almost entirely away from using ball color for
    // image-processing.
    // This stuff is deprecated.
    enum BallColor
    {
        kCalibrated = 0,      // If set to this calibrated psuedo-color, then
                              // the ball's average_color_ is the actual best
                              // description of the color
        kWhite = 1,
        kOrange = 2,
        kYellow = 3,
        kOpticGreen = 4,
        kUnknown = 5
    };

    // If the ball's hsv range is known (at it really better be!), ball_color_
    // should be set to kCalibrated
    BallColor ball_color_ = BallColor::kUnknown;  // enum BallColor

    // All zero's signifies that there is no value set yet
    cv::Scalar average_color_;                  // RGB Triplet (stored in BGR
                                                // order as per openCV)
    cv::Scalar median_color_;               // RGB Triplet (stored in BGR order
                                            // as per openCV)
    cv::Scalar std_color_;                  // Triplet (stored in BGR order as
                                            // per openCV)

    // The screen-image region in which the ball is expected to be found.  May
    // not be set, and if so will be all 0's
    cv::Rect expected_ROI_;

    double distance_at_calibration_ = -1.0;     // In meters at the time of
                                                // calibration
    double radius_at_calibration_pixels_ = -1.0;       // In pixels.  Should
                                                       // only be set after
                                                       // calibration
    double calibrated_focal_length_ = -1.0;     // In mm.  This is set if we can
                                                // get a precise, known distance
                                                // to the ball
    // if so, we can compute the focal distance based on the known ball size and
    // radius-in-pixels at the known distance
    bool calibrated = false;

    uint quality_ranking = 0;   // 0 is best.  Set by circle/ellipse detector if
                                // possible

    cv::Vec3d rotation_speeds_RPM_;
    double velocity_ = 0; // In m/s
    long time_between_ball_positions_for_velocity_uS_ = 0;
    long time_between_angle_measures_for_rpm_uS_ = 0;

    // This next variable may be important to help create a good colorMask that
    // will remove unwanted parts
    // of the image while still preserving the likely ball portion of the iamge
    // If set to kCalibrated, the ball_hsv_range_ must be set
    // TBD - Note the condition toward the red end of the scale where the HSV
    // range may
    // 'loop' around 0 and 180 degrees!
    BallColorRange ball_hsv_range_;

    // These next two properties apply for balls that were searched-for in a
    // particular
    // area
    cv::Vec2i search_area_center_;
    int search_area_radius_ = 0;


    // Again, we're moving away from using the ball color for processing in most
    // instances
    cv::Scalar GetBallLowerHSV(BallColor ball_color) const;
    cv::Scalar GetBallUpperHSV(BallColor ball_color) const;

    // Return the expected ball color from the coarse ball color settings.
    // Returns RGB value from the BallHSVRangeDict
    cv::Scalar GetRGBCenterFromHSVRange() const;

    // Format the ball into a string
    std::string Format() const;

    void PrintBallFlightResults() const;

    // Returns true if this ball moved (in any direction, so that includes the
    // center and
    // the radius) relative to the ball_to_compare.
    // Max-percent figures are 0 to 100, but can also be greater than 100%.
    bool CheckIfBallMoved(const GolfBall &ball_to_compare,
                          const int max_center_move_pixels,
                          const int max_radius_change_percent);

    double PixelDistanceFromBall(const GolfBall &ball2) const;

    static void AverageBalls(const std::vector<GolfBall> &ball_vector, GolfBall &averaged_ball);

    bool PointIsInsideBall(const double x, const double y) const;

  private:

    long x_ = 0;                         // Position on screen.  In pixels in
                                         // openCV coordinate system
    long y_ = 0;                         // In pixels in openCV coordinate
                                         // system

    // (De)Initialize any members -- called from (de)constructors.
    void InitMembers();
    void DeInitMembers();
};
}

#endif // GOLF_BALL_H
