#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

namespace PiTrac
{
enum class SystemStatus_Type
{
    INITIALIZING = 0,
    IDLE,
    VIEWFINDING,
    CALIBRATING,
    WAITING,
    CAPTURING,
    PROCESSING,
    ERROR_HANDLING,
    MAX_STATUS
};

enum class SystemMode_Type
{
    STANDBY = 0,
    VIEWFINDER,
    CALIBRATION,
    LAUNCH_MONITOR,
    DIAGNOSTICS,
    MAX_MODE
};

enum class EventID_Type
{
    AwaitingStrike,
    StrikeDetected,
    DataProcessed,
    ErrorOccurred,
    MAX_EVENT_ID
};

enum class LaunchMonitorState_Type
{
    ACQUIRING_BALL = 0,
    AWAITING_STRIKE,
    PROCESSING_STRIKE,
    ERROR,
    MAX_STATE
};

} // namespace PiTrac

#endif // SYSTEM_MODES_H