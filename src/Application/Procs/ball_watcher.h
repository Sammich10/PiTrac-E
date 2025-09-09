/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022-2025, Verdant Consultants, LLC.
 */

#pragma once

#include "core/rpicam_encoder.hpp"
#include "encoder/encoder.hpp"

namespace PiTrac
{
// The main event loop
// Returns true if function ran as expected, and without error
// motion_detected will be set true only if motion was successfully detected.
bool ball_watcher_event_loop
(
    RPiCamEncoder &app,
    bool &motion_detected
);
}
