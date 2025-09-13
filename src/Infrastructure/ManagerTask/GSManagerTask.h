#ifndef GSMANAGERTASK_H
#define GSMANAGERTASK_H

#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "Infrastructure/ManagerTask/GSManagerTask.h"
#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include <vector>
#include <memory>

namespace PiTrac
{
class GSManagerTask : public GSTaskBase

{
  protected:
    // @brief Managers to run in this task/process
    std::vector<std::shared_ptr<GSManagerBase> > managers_;

    // @brief Manager-specific configuration
    bool restart_failed_managers_;

    // @brief Interval at which the manager performs checks
    std::chrono::milliseconds manager_check_interval_;

    std::string manager_task_ipc_endpoint_;

  public:

    /**
     * @brief Constructs a GSManagerTask object with the specified name.
     *
     * @param name The name to assign to the manager task.
     */
    GSManagerTask
    (
        const std::string &name
    );

    /**
     * @brief Virtual destructor for GSManagerTask.
     *
     * Ensures proper cleanup of derived classes when deleted through a base
     * class pointer.
     */
    virtual ~GSManagerTask();


    /**
     * @brief Adds an manager to the task.
     *
     * @param manager A shared pointer to a GSManagerBase instance representing
     *the
     * manager to be added.
     */
    void addManager
    (
        std::shared_ptr<GSManagerBase> manager
    );

    /**
     * @brief Removes an manager from the system by its identifier.
     *
     * This function removes the manager specified by the given manager ID.
     *
     * @param manager_id The unique identifier of the manager to be removed.
     */
    void removeManager
    (
        const std::string &manager_id
    );

    /**
     * @brief Retrieves a list of manager objects managed by this class.
     *
     * @return A vector containing shared pointers to GSManagerBase instances.
     */
    std::vector<std::shared_ptr<GSManagerBase> > getManagers() const;

    /**
     * @brief Returns the number of managers currently managed.
     *
     * @return The count of managers.
     */
    size_t getManagerCount() const
    {
        return managers_.size();
    }

    /**
     * @brief Sets whether failed managers should be restarted.
     *
     * This method updates the internal flag that determines if managers
     * that have failed should be restarted automatically.
     *
     * @param restart If true, failed managers will be restarted; otherwise,
     *they
     * will not.
     */
    void setRestartFailedManagers(bool restart)
    {
        restart_failed_managers_ = restart;
    }

    /**
     * @brief Sets the interval at which the manager performs checks.
     *
     * @param interval The time interval, in milliseconds, between manager
     *checks.
     */
    void setManagerCheckInterval(std::chrono::milliseconds interval)
    {
        manager_check_interval_ = interval;
    }

  protected:

    /**
     * @brief Sets up all managers required by the system.
     *
     * This function performs the necessary setup operations for all managers,
     * ensuring they are properly configured and ready for use.
     *
     * @return true if all managers were successfully set up; false otherwise.
     */
    bool setupAllManagers();

    /**
     * @brief Configures the managers to be managed by this task.
     *
     * This pure virtual function must be implemented by derived classes to
     * specify
     * which managers should be created and managed within this task. The
     * implementation
     * should create instances of GSManagerBase-derived classes and add them to
     * the
     * internal managers_ vector using the addManager() method.
     */
    virtual void configureManagers() = 0;

    /**
     * @brief Starts all managers managed by this class.
     *
     * This function initiates the startup process for all managers.
     *
     * @return true if all managers started successfully, false otherwise.
     */
    bool startAllManagers();

    /**
     * @brief Stops all running managers managed by this class.
     *
     * This method terminates or halts the execution of all managers currently
     * active.
     * It is typically used to ensure a clean shutdown or to reset the manager
     * system.
     */
    void stopAllManagers();

    /**
     * @brief Checks if all managers are currently running.
     *
     * This function determines whether every manager in the system is in a
     * running state.
     *
     * @return true if all managers are running; false otherwise.
     */
    bool areAllManagersRunning();

    /**
     * @brief Restarts managers that have previously failed.
     *
     * This method identifies managers that are in a failed state and attempts
     *to
     * restart them.
     * It is typically used to recover from transient errors or to ensure
     * continued operation
     * of all managers within the system.
     */
    void restartFailedManagers();

    /**
     * @brief Processes the main logic for the manager task.
     *
     * This method overrides the base class implementation to execute
     * the core functionality required by the manager task. It should
     * contain the main processing loop or logic specific to this manager.
     */
    void processMain() override;


    /**
     * @brief Cleans up resources or performs necessary finalization for the
     * process.
     *
     * This method is called to handle any cleanup operations required after the
     * process
     * has completed its execution. Override this method to implement custom
     * cleanup logic.
     */
    void cleanupProcess() override
    {
    }

    /**
     * @brief Hook method called before the manager starts.
     *
     * This virtual function can be overridden to perform any necessary
     * setup or checks before the manager begins its execution.
     *
     * @return true if the manager can proceed with starting; false otherwise.
     */
    virtual bool preManagerStartHook()
    {
        return true;
    }

    /**
     * @brief Hook method called after the manager has started.
     *
     * Override this method to implement custom logic that should be executed
     * immediately after the manager startup process completes.
     */
    virtual void postManagerStartHook()
    {
    }

    /**
     * @brief Hook method called when an manager fails.
     *
     * This virtual function can be overridden to implement custom behavior
     * when the specified manager encounters a failure.
     *
     * @param manager Shared pointer to the manager that has failed.
     */
    virtual void onManagerFailedHook(std::shared_ptr<GSManagerBase> manager)
    {
    }

    /**
     * @brief Hook method called during the manager's monitoring loop.
     *
     * This virtual function can be overridden by derived classes to implement
     * custom behavior that should occur during each iteration of the manager's
     * monitoring loop.
     */
    virtual void managerMonitoringLoopHook()
    {
    }

    std::unique_ptr<GSMessagerBase> manager_task_ipc_subscriber_;
};
} // namespace PiTrac

#endif // GSMANAGERTASK_H