#ifndef GSManager_H
#define GSManager_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messages/ChangeModeMsg.h"
#include "Foundation/TaskProcess/TaskBase.h"
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
class GSManagerBase : public TaskBase
{
  public:
    GSManagerBase
    (
        const std::string &name
    )
        : TaskBase(name)
    {
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