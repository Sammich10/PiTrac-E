#include "Infrastructure/AgentTask/GSAgentTask.h"

namespace PiTrac {

GSAgentTask::GSAgentTask(const std::string& name)
    : GSTaskBase(name)
    , restart_failed_agents_(false)
    , agent_check_interval_(std::chrono::milliseconds(1000)) {
}

GSAgentTask::~GSAgentTask() {
    // Agents will be cleaned up by base destructor
}

bool GSAgentTask::start() {
    // Use base start but add agent-specific checks
    return GSTaskBase::start();
}

bool GSAgentTask::preStartHook() {
    // Configure agents before starting
    try {
        configureAgents();
        logInfo("Configured " + std::to_string(agents_.size()) + " agents");
        return true;
    } catch (const std::exception& e) {
        logError("Failed to configure agents: " + std::string(e.what()));
        return false;
    }
}

void GSAgentTask::childProcessMain() {
    logInfo("Starting agent task main loop");
    
    // Pre-agent start hook
    if (!preAgentStartHook()) {
        logError("Pre-agent start hook failed");
        return;
    }
    
    // Start all agents
    if (!startAllAgents()) {
        logError("Failed to start agents");
        return;
    }
    
    postAgentStartHook();
    
    // Main monitoring loop
    while (!should_stop_) {
        // Check agent health
        if (!areAllAgentsRunning()) {
            if (restart_failed_agents_) {
                restartFailedAgents();
            } else {
                logError("Some agents failed and restart is disabled");
                break;
            }
        }
        
        // Custom hook for derived classes
        agentMonitoringLoopHook();
        
        std::this_thread::sleep_for(agent_check_interval_);
    }
    
    logInfo("Agent task main loop ended");
    stopAllAgents();
}

bool GSAgentTask::startAllAgents() {
    logInfo("Starting " + std::to_string(agents_.size()) + " agents");
    
    for (auto& agent : agents_) {
        if (!agent->start()) {
            logError("Failed to start agent: " + agent->getAgentName());
            return false;
        }
    }
    
    logInfo("All agents started successfully");
    return true;
}

void GSAgentTask::stopAllAgents() {
    logInfo("Stopping all agents");
    
    for (auto& agent : agents_) {
        agent->stop();
    }
    
    // Wait for agents to stop
    for (auto& agent : agents_) {
        agent->waitForCompletion(std::chrono::seconds(5));
    }
    
    logInfo("All agents stopped");
}

bool GSAgentTask::areAllAgentsRunning() {
    for (const auto& agent : agents_) {
        if (!agent->isRunning()) {
            return false;
        }
    }
    return true;
}

void GSAgentTask::restartFailedAgents() {
    for (auto& agent : agents_) {
        if (!agent->isRunning()) {
            logWarning("Restarting failed agent: " + agent->getAgentName());
            onAgentFailedHook(agent);
            
            if (!agent->start()) {
                logError("Failed to restart agent: " + agent->getAgentName());
            }
        }
    }
}

void GSAgentTask::addAgent(std::shared_ptr<GSAgentBase> agent) {
    agents_.push_back(agent);
    logInfo("Added agent: " + agent->getAgentName() + " to task: " + task_name_);
}

void GSAgentTask::removeAgent(const std::string& agent_id) {
    auto it = std::remove_if(agents_.begin(), agents_.end(),
        [&agent_id](const std::shared_ptr<GSAgentBase>& agent) {
            return agent->getAgentId() == agent_id;
        });
    
    if (it != agents_.end()) {
        agents_.erase(it, agents_.end());
        logInfo("Removed agent: " + agent_id + " from task: " + task_name_);
    }
}

std::vector<std::shared_ptr<GSAgentBase>> GSAgentTask::getAgents() const {
    return agents_;
}

} // namespace PiTrac