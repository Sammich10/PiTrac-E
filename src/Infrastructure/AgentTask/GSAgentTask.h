#ifndef GSAGENTTASK_H
#define GSAGENTTASK_H

#include "Infrastructure/TaskProcess/GSTaskBase.h"
#include "Infrastructure/AgentTask/GSAgentTask.h"
#include "Application/AppAgents/AgentBase/GSAgentBase.h"
#include <vector>
#include <memory>

namespace PiTrac {

class GSAgentTask : public GSTaskBase {
protected:
    // Agents to run in this task/process
    std::vector<std::shared_ptr<GSAgentBase>> agents_;
    
    // Agent-specific configuration
    bool restart_failed_agents_;
    std::chrono::milliseconds agent_check_interval_;
    
public:
    GSAgentTask(const std::string& name);
    virtual ~GSAgentTask();
    
    // Pure virtual method for agent configuration
    virtual void configureAgents() = 0;        // Add agents to this task
    
    // Agent management
    void addAgent(std::shared_ptr<GSAgentBase> agent);
    void removeAgent(const std::string& agent_id);
    std::vector<std::shared_ptr<GSAgentBase>> getAgents() const;
    size_t getAgentCount() const { return agents_.size(); }
    
    // Agent task configuration
    void setRestartFailedAgents(bool restart) { restart_failed_agents_ = restart; }
    void setAgentCheckInterval(std::chrono::milliseconds interval) { 
        agent_check_interval_ = interval; 
    }
    
    // Override base task methods with agent-specific behavior
    bool start() override;
    
protected:
    // Agent-specific process management
    bool startAllAgents();
    void stopAllAgents();
    bool areAllAgentsRunning();
    void restartFailedAgents();
    
    // Default implementation of childProcessMain for agent tasks
    void childProcessMain() override;
    
    // Hook methods for agent-specific customization
    virtual bool preAgentStartHook() { return true; }     // Called before starting agents
    virtual void postAgentStartHook() {}                  // Called after agents started
    virtual void onAgentFailedHook(std::shared_ptr<GSAgentBase> agent) {}  // Called when agent fails
    virtual void agentMonitoringLoopHook() {}             // Called in each monitoring loop iteration
    
    // Override base hooks to include agent configuration
    bool preStartHook() override;
};

} // namespace PiTrac

#endif // GSAGENTTASK_H