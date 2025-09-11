#ifndef GSAGENTTASK_H
#define GSAGENTTASK_H

#include "Application/AppAgents/AgentBase/GSAgentBase.h"
#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "Infrastructure/AgentTask/GSAgentTask.h"
#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include <vector>
#include <memory>

namespace PiTrac
{
class GSAgentTask : public GSTaskBase
/**
 * @class GSAgentTask
 * @brief Manages a collection of agents within a task/process, providing
 * lifecycle control and monitoring.
 *
 * The GSAgentTask class serves as an abstract base for managing multiple agents
 * derived from GSAgentBase.
 * It provides mechanisms for agent configuration, addition/removal, lifecycle
 * management (start/stop/restart),
 * and monitoring. The class supports hooks for customization at various stages
 * of agent operation, such as
 * before/after start, on failure, and during monitoring loops. Derived classes
 * must implement the
 * configureAgents() method to specify which agents are managed by the task.
 *
 * Key Features:
 * - Manages a vector of agents, allowing dynamic addition and removal.
 * - Supports automatic restart of failed agents, configurable via
 * setRestartFailedAgents().
 * - Allows customization of agent check intervals.
 * - Provides hooks for extending behavior at critical points in the agent
 * lifecycle.
 * - Offers methods to start, stop, and monitor all managed agents.
 * - Integrates with process lifecycle via setupProcess(), cleanupProcess(), and
 * processMain().
 *
 * Usage:
 * - Inherit from GSAgentTask and implement configureAgents() to specify agents.
 * - Optionally override hook methods to customize agent behavior.
 * - Use start(), stopAllAgents(), and related methods to control agent
 * execution.
 */
{
  protected:
    // @brief Agents to run in this task/process
    std::vector<std::shared_ptr<GSAgentBase> > agents_;

    // @brief Agent-specific configuration
    bool restart_failed_agents_;

    // @brief Interval at which the agent performs checks
    std::chrono::milliseconds agent_check_interval_;

    std::string agent_ipc_endpoint_;

  public:

    /**
     * @brief Constructs a GSAgentTask object with the specified name.
     *
     * @param name The name to assign to the agent task.
     */
    GSAgentTask
    (
        const std::string &name
    );

    /**
     * @brief Virtual destructor for GSAgentTask.
     *
     * Ensures proper cleanup of derived classes when deleted through a base
     * class pointer.
     */
    virtual ~GSAgentTask();

    /**
     * @brief Configures the agents to be managed by this task.
     *
     * This pure virtual function must be implemented by derived classes to
     * specify
     * which agents should be created and managed within this task. The
     * implementation
     * should create instances of GSAgentBase-derived classes and add them to
     * the
     * internal agents_ vector using the addAgent() method.
     */
    virtual void configureAgents() = 0;

    /**
     * @brief Adds an agent to the task.
     *
     * @param agent A shared pointer to a GSAgentBase instance representing the
     * agent to be added.
     */
    void addAgent
    (
        std::shared_ptr<GSAgentBase> agent
    );

    /**
     * @brief Removes an agent from the system by its identifier.
     *
     * This function removes the agent specified by the given agent ID.
     *
     * @param agent_id The unique identifier of the agent to be removed.
     */
    void removeAgent
    (
        const std::string &agent_id
    );

    /**
     * @brief Retrieves a list of agent objects managed by this class.
     *
     * @return A vector containing shared pointers to GSAgentBase instances.
     */
    std::vector<std::shared_ptr<GSAgentBase> > getAgents() const;

    /**
     * @brief Returns the number of agents currently managed.
     *
     * @return The count of agents.
     */
    size_t getAgentCount() const
    {
        return agents_.size();
    }

    /**
     * @brief Sets whether failed agents should be restarted.
     *
     * This method updates the internal flag that determines if agents
     * that have failed should be restarted automatically.
     *
     * @param restart If true, failed agents will be restarted; otherwise, they
     * will not.
     */
    void setRestartFailedAgents(bool restart)
    {
        restart_failed_agents_ = restart;
    }

    /**
     * @brief Sets the interval at which the agent performs checks.
     *
     * @param interval The time interval, in milliseconds, between agent checks.
     */
    void setAgentCheckInterval(std::chrono::milliseconds interval)
    {
        agent_check_interval_ = interval;
    }

    /**
     * @brief Starts the agent task.
     *
     * This method initiates the execution of the agent task. It should be
     * overridden
     * by derived classes to provide specific start-up logic.
     *
     * @return true if the task started successfully, false otherwise.
     */
    bool start() override;

  protected:

    /**
     * @brief Sets up all agents required by the system.
     *
     * This function performs the necessary setup operations for all agents,
     * ensuring they are properly configured and ready for use.
     *
     * @return true if all agents were successfully set up; false otherwise.
     */
    bool setupAllAgents();
    /**
     * @brief Starts all agents managed by this class.
     *
     * This function initiates the startup process for all agents.
     *
     * @return true if all agents started successfully, false otherwise.
     */
    bool startAllAgents();
    /**
     * @brief Stops all running agents managed by this class.
     *
     * This method terminates or halts the execution of all agents currently
     * active.
     * It is typically used to ensure a clean shutdown or to reset the agent
     * system.
     */
    void stopAllAgents();

    /**
     * @brief Checks if all agents are currently running.
     *
     * This function determines whether every agent in the system is in a
     * running state.
     *
     * @return true if all agents are running; false otherwise.
     */
    bool areAllAgentsRunning();

    /**
     * @brief Restarts agents that have previously failed.
     *
     * This method identifies agents that are in a failed state and attempts to
     * restart them.
     * It is typically used to recover from transient errors or to ensure
     * continued operation
     * of all agents within the system.
     */
    void restartFailedAgents();

    /**
     * @brief Processes the main logic for the agent task.
     *
     * This method overrides the base class implementation to execute
     * the core functionality required by the agent task. It should
     * contain the main processing loop or logic specific to this agent.
     */
    void processMain() override;

    /**
     * @brief Sets up the process for the agent task.
     *
     * This method is called to perform any necessary setup before the agent
     * task begins execution.
     *
     * @return true if the setup was successful, false otherwise.
     */
    bool setupProcess() override
    {
        return true;
    }

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
     * @brief Hook method called before the agent starts.
     *
     * This virtual function can be overridden to perform any necessary
     * setup or checks before the agent begins its execution.
     *
     * @return true if the agent can proceed with starting; false otherwise.
     */
    virtual bool preAgentStartHook()
    {
        return true;
    }

    /**
     * @brief Hook method called after the agent has started.
     *
     * Override this method to implement custom logic that should be executed
     * immediately after the agent startup process completes.
     */
    virtual void postAgentStartHook()
    {
    }

    /**
     * @brief Hook method called when an agent fails.
     *
     * This virtual function can be overridden to implement custom behavior
     * when the specified agent encounters a failure.
     *
     * @param agent Shared pointer to the agent that has failed.
     */
    virtual void onAgentFailedHook(std::shared_ptr<GSAgentBase> agent)
    {
    }

    /**
     * @brief Hook method called during the agent's monitoring loop.
     *
     * This virtual function can be overridden by derived classes to implement
     * custom behavior that should occur during each iteration of the agent's
     * monitoring loop.
     */
    virtual void agentMonitoringLoopHook()
    {
    }

    /**
     * @brief Hook method called before the agent task is started.
     *
     * This virtual function can be overridden by derived classes to implement
     * custom behavior that should occur immediately before the agent task
     * starts.
     * The default implementation does nothing.
     */
    bool preStartHook() override;

    std::unique_ptr<GSMessagerBase> agent_ipc_subscriber_;
};
} // namespace PiTrac

#endif // GSAGENTTASK_H