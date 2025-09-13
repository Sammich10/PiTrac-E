#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

namespace PiTrac
{
enum class SystemMode
{
    Initializing,
    Idle,
    Viewfinding,
    Calibrating,
    Waiting,
    CameraCapture,
    DataProcessing,
    ErrorHandling
};

enum class EventID
{
    AwaitingStrike,
    StrikeDetected,
    DataProcessed,
    ErrorOccurred
};

enum class LaunchMonitorState
{
    ACQUIRING_BALL = 0,
    AWAITING_STRIKE,
    PROCESSING_STRIKE,
    ERROR,
    MAX
};
} // namespace PiTrac

#endif // SYSTEM_MODES_H