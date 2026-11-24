#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

#include <string>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>


namespace PiTrac
{
enum SystemMode_Type
{
    STANDBY = 0,
    CALIBRATION,
    LAUNCH_MONITOR,
    DIAGNOSTIC,
    MAX_MODE,
    INVALID_MODE = MAX_MODE
};

enum EventID_Type
{
    None = 0,
    AwaitingStrike,
    StrikeDetected,
    DataProcessed,
    ErrorOccurred,
    MAX_EVENT_ID
};
/**
 * @brief SystemStatus class encapsulates the current mode, state, and events of the system. It
 * exists in shared memory for all processes to access and synchronize system status.
 */
class SystemStatus
{
  public:
    static SystemStatus *getInstance();

    static const std::string systemModeToString(SystemMode_Type mode)
    {
        switch (mode)
        {
            case SystemMode_Type::STANDBY:
                return "STANDBY";
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
        if (mode_str == "STANDBY")
        {
            return SystemMode_Type::STANDBY;
        }
        else if (mode_str == "CALIBRATION")
        {
            return SystemMode_Type::CALIBRATION;
        }
        else if (mode_str == "LAUNCH_MONITOR")
        {
            return SystemMode_Type::LAUNCH_MONITOR;
        }
        else if (mode_str == "DIAGNOSTIC")
        {
            return SystemMode_Type::DIAGNOSTIC;
        }
        else
        {
            return SystemMode_Type::MAX_MODE; // Unknown mode
        }
    }

    SystemStatus();

    ~SystemStatus();
  protected:


    SystemMode_Type currentMode_;
    EventID_Type currentEvent_;

    static SystemStatus *instance_;
};
} // namespace PiTrac

#endif // SYSTEM_MODES_H