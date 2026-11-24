#include "Common/System/System.h"

namespace PiTrac
{
SystemStatus *SystemStatus::instance_ = nullptr;

SystemStatus::SystemStatus() :
    currentMode_(SystemMode_Type::MAX_MODE),
    currentEvent_(EventID_Type::None)
{
}

SystemStatus::~SystemStatus()
{
}

SystemStatus *SystemStatus::getInstance()
{
    if (!instance_)
    {
        const uint64_t shared_mem = sizeof(SystemStatus) + 4096;
        static boost::interprocess::managed_shared_memory segment(boost::interprocess::open_or_create, "SystemStatusSegment", shared_mem);
        instance_ = segment.find_or_construct<SystemStatus>("SystemStatus")();
    }
    return instance_;
}
} // namespace PiTrac