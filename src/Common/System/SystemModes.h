#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

namespace PiTrac
{
enum class SystemStatus
{
    Initializing,
    Idle,
    Viewfinding,
    Calibrating,
    Waiting,
    CameraCapture,
    DataProcessing,
    ErrorHandling,
    MAX_STATUS
};

enum class SystemMode
{
    Standby = 0,
    Viewfinder,
    Calibration,
    LaunchMonitor,
    Diagnostics,
    MAX_MODE
}

enum class EventID
{
    AwaitingStrike,
    StrikeDetected,
    DataProcessed,
    ErrorOccurred,
    MAX_EVENT_ID
};

enum class LaunchMonitorState
{
    ACQUIRING_BALL = 0,
    AWAITING_STRIKE,
    PROCESSING_STRIKE,
    ERROR,
    MAX_STATE
};

} // namespace PiTrac

#endif // SYSTEM_MODES_H