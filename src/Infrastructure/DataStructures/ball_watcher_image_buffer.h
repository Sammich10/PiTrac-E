/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022-2025, Verdant Consultants, LLC.
 */

// This structure is setup by the libcamera loop with the (usually)
// rapidly-taken
// images from the camera.

#ifndef GS_BALL_WATCHER_IMG_BUFFER_H
#define GS_BALL_WATCHER_IMG_BUFFER_H

#include <opencv4/opencv2/core/cvdef.h>
#include <opencv4/opencv2/highgui.hpp>

#include <boost/circular_buffer.hpp>

namespace PiTrac
{
//  We also need to be able to reach these variables from within the libcamera
// namespace.

struct RecentFrameInfo
{
    cv::Mat mat;
    // Holds the sequence number from the completed request from whence the mat
    // came
    unsigned int requestSequence;
    // True if this was the frame where motion (the ball hit) was first detected
    bool isballHitFrame = false;
    float frameRate = 0.0;
};

// Global queue to hold the last <n> frames before motion is detected in the
// frame
extern boost::circular_buffer<RecentFrameInfo> RecentFrames;
}

#endif // GS_BALL_WATCHER_IMG_BUFFER_H