#ifndef GSManager_H
#define GSManager_H

#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"
#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <iomanip>

namespace PiTrac
{
class GSManagerBase : public GSTaskBase
{
  public:
    GSManagerBase
    (
        const std::string &name
    )
        : GSTaskBase(name)
    {
        logInfo("Manager created: " + name_ + " [" + task_id_ + "]");
    }

    ~GSManagerBase()
    {
        logInfo("Manager destroyed: " + name_);
    }

  protected:


    // Override processMain, calls execute in a new thread. This will be
    // the main entry point for the manager.
    void processMain() override
    {
        if (getStatus() == TaskStatus::Running)
        {
            logWarning("Manager already running: " + name_);
            return;
        }

        if (getStatus() == TaskStatus::Paused)
        {
            changeStatus(TaskStatus::Running);
            logInfo("Resuming manager: " + name_);
            return;
        }

        execute();
    }

    virtual bool execute
    (
        void
    ) = 0;
};
} // namespace PiTrac

#endif // GSManager_H