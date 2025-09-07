#include "Interfaces/Camera/imx296/imx296.h"

namespace PiTrac
{
bool IMX296Camera::configureTriggerMode(const TriggerMode &mode)
{
    if (!camera_ || !config_)
    {
        return false;
    }

    // Set trigger mode controls
    libcamera::ControlList controls;

    if (triggerMode_ == TriggerMode::EXTERNAL_TRIGGER)
    {
        // Configure for external trigger
        // Note: Exact control names depend on your libcamera/driver
        // implementation
        controls.set(libcamera::controls::FrameDurationLimits, {INT64_MAX, INT64_MAX}); // No
                                                                                        // frame
                                                                                        // rate
                                                                                        // limit
        // controls.set(libcamera::controls::TriggerMode, 1); // Enable external
        // trigger (if supported)
    }
    else
    {
        // Configure for free running
        int64_t frameDuration = static_cast<int64_t>(1000000.0f / currentFps_); // microseconds
        controls.set(libcamera::controls::FrameDurationLimits, {frameDuration, frameDuration});
        // controls.set(libcamera::controls::TriggerMode, 0); // Disable
        // external trigger
    }

    // Apply controls to all requests
    for (auto &request : requests_)
    {
        request->controls().merge(controls);
    }

    return true;
}

std::string IMX296Camera::toString() const
{
    return "IMX296Camera [" + std::to_string(resolutionX_) + "x" + std::to_string(resolutionY_) +
           ", FL:" + std::to_string(focalLength_mm_) + "mm]";
}
} // namespace PiTrac