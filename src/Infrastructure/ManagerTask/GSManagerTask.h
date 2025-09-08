#ifndef GSMANAGERTASK_H
#define GSMANAGERTASK_H

#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include <vector>
#include <memory>
#include <unordered_map>

namespace PiTrac
{
// Forward declaration for manager interface
class GSManagerInterface
{
  public:
    virtual ~GSManagerInterface() = default;
    virtual bool initialize() = 0;
    virtual void execute() = 0;
    virtual void cleanup() = 0;
    virtual bool isHealthy() = 0;
    virtual std::string getManagerName() const = 0;
};

class GSManagerTask : public GSTaskBase
{
  protected:
    // Manager to run in this task/process
    std::unique_ptr<GSManagerInterface> manager_;

    // Messaging infrastructure for managers
    std::shared_ptr<GSMessagerBase> input_messager_;
    std::shared_ptr<GSMessagerBase> output_messager_;

    // Manager-specific configuration
    std::chrono::milliseconds health_check_interval_;
    bool auto_restart_on_failure_;

  public:
    GSManagerTask(const std::string &name);
    virtual ~GSManagerTask();

    // Pure virtual method for manager configuration
    virtual void configureManager() = 0;       // Create and configure the
                                               // manager
    virtual void configureMessaging() = 0;     // Set up messaging
                                               // infrastructure

    // Manager management
    void setManager(std::unique_ptr<GSManagerInterface> manager);
    GSManagerInterface *getManager() const
    {
        return manager_.get();
    }

    // Messaging configuration
    void setInputMessager(std::shared_ptr<GSMessagerBase> messager)
    {
        input_messager_ = messager;
    }

    void setOutputMessager(std::shared_ptr<GSMessagerBase> messager)
    {
        output_messager_ = messager;
    }

    std::shared_ptr<GSMessagerBase> getInputMessager() const
    {
        return input_messager_;
    }

    std::shared_ptr<GSMessagerBase> getOutputMessager() const
    {
        return output_messager_;
    }

    // Manager task configuration
    void setHealthCheckInterval(std::chrono::milliseconds interval)
    {
        health_check_interval_ = interval;
    }

    void setAutoRestartOnFailure(bool restart)
    {
        auto_restart_on_failure_ = restart;
    }

    // Override base task methods with manager-specific behavior
    bool start() override;

  protected:
    // Default implementation of childProcessMain for manager tasks
    void childProcessMain() override;

    // Manager-specific process management
    bool startManager();
    void stopManager();
    bool isManagerHealthy();
    void restartManager();

    // Hook methods for manager-specific customization
    virtual bool preManagerStartHook()
    {
        return true;
    }                                                     // Called before
                                                          // starting manager

    virtual void postManagerStartHook()
    {
    }                                                     // Called after
                                                          // manager started

    virtual void onManagerFailedHook()
    {
    }                                                     // Called when manager
                                                          // fails

    virtual void managerMonitoringLoopHook()
    {
    }                                                     // Called in each
                                                          // monitoring loop
                                                          // iteration

    // Override base hooks to include manager configuration
    bool preStartHook() override;
};
} // namespace PiTrac

#endif // GSMANAGERTASK_H