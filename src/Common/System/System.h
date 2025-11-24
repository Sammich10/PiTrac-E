#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

#include <string>

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
    STARTING_UP = 0,
    STANDBY,
    VIEWFINDER,
    CALIBRATION,
    LAUNCH_MONITOR,
    DIAGNOSTIC,
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

class System
{
  public:
    System() = delete;
    ~System() = delete;

    static const std::string systemModeToString(SystemMode_Type mode)
    {
        switch (mode)
        {
            case SystemMode_Type::STARTING_UP:
                return "STARTING_UP";
            case SystemMode_Type::STANDBY:
                return "STANDBY";
            case SystemMode_Type::VIEWFINDER:
                return "VIEWFINDER";
            case SystemMode_Type::CALIBRATION:
                return "CALIBRATION";
            case SystemMode_Type::LAUNCH_MONITOR:
                return "LAUNCH_MONITOR";
            case SystemMode_Type::DIAGNOSTIC:
                return "DIAGNOSTIC";
            default:
                return "UNKNOWN_MODE";
        }
    }

    static const SystemMode_Type stringToSystemMode(const std::string &mode_str)
    {
        if (mode_str == "STARTING_UP")
            return SystemMode_Type::STARTING_UP;
        else if (mode_str == "STANDBY")
            return SystemMode_Type::STANDBY;
        else if (mode_str == "VIEWFINDER")
            return SystemMode_Type::VIEWFINDER;
        else if (mode_str == "CALIBRATION")
            return SystemMode_Type::CALIBRATION;
        else if (mode_str == "LAUNCH_MONITOR")
            return SystemMode_Type::LAUNCH_MONITOR;
        else if (mode_str == "DIAGNOSTIC")
            return SystemMode_Type::DIAGNOSTIC;
        else
            return SystemMode_Type::MAX_MODE; // Unknown mode
    }
};
} // namespace PiTrac

#endif // SYSTEM_MODES_H