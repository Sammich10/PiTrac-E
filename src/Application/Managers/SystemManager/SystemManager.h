#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include "Infrastructure/Messaging/Messagers/MessageRouter.h"
#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messages/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/SystemCommandMsg.h"
#include "Infrastructure/Messaging/Messages/AckMessage.h"
#include "Infrastructure/Messaging/Messages/RegisterTaskMsg.h"
#include "Infrastructure/Messaging/Messages/HeartbeatMsg.h"
#include "Common/System/System.h"
#include <chrono>
#include <map>

namespace PiTrac
{
class SystemManager : public GSManagerBase
{
  public:
    SystemManager();
    virtual ~SystemManager();

  protected:
    std::unique_ptr<MessageRouter> task_control_router_;
    std::unique_ptr<MessagerBase> system_command_listener_;

    bool setupProcess() override;
    void cleanupProcess() override;
    bool execute() override;

  private:
    struct RegisteredAgent
    {
        std::string task_name;
        uint64_t task_pid;
        std::string zmq_identity;
        std::chrono::system_clock::time_point last_seen;
        SystemMode_Type current_mode = SystemMode_Type::MAX_MODE;

        RegisteredAgent() = default;
        RegisteredAgent(const std::string &name, uint64_t pid, const std::string &identity)
            : task_name(name)
            , task_pid(pid)
            , zmq_identity(identity)
            , last_seen(std::chrono::system_clock::now())
        {
        }
    };

    // Map identities with registered agents
    std::map<std::string, RegisteredAgent> registered_agents_;
    // Map agent names to their ZeroMQ identities
    std::map<std::string, std::string> agent_name_to_identity_;
    std::mutex agents_mutex_;
    std::mutex router_mutex_; 

    // Message handlers
    void taskControlMessageHandler
    (
        std::unique_ptr<MessagerBase::IdentityMessage> identity_message
    );
    void externalMessageHandler
    (
        std::unique_ptr<MessageInterface> message
    );

    // Agent management
    void handleAgentRegistration
    (
        const std::string &identity,
        const RegisterTaskMsg &reg_msg
    );
    void handleAgentHeartbeat
    (
        const std::string &identity,
        const HeartbeatMsg &heartbeat
    );
    void sendAcknowledgmentToAgent
    (
        const std::string &identity,
        const MessageInterface &original_message,
        const bool success = true
    );

    void sendAcknowledgementToHost
    (
        const MessageInterface &original_message, 
        const bool success = true
    );

    // Command distribution
    void sendModeChangeToAgent
    (
        const std::string &identity,
        SystemMode_Type new_mode
    );
    void broadcastModeChange
    (
        SystemMode_Type new_mode
    );

    // Agent monitoring
    void checkAgentTimeouts();
    std::vector<RegisteredAgent> getActiveAgents();

    // External command handling
    bool handleModeChangeCommand
    (
        const SystemCommandMsg &cmd_msg
    );

    // Current system mode
    SystemMode_Type mode_ = SystemMode_Type::STARTING_UP;
};
} // namespace PiTrac

#endif // ENHANCED_SYSTEM_MANAGER_H