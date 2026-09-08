#ifndef SYSTEM_MANAGER_H
#define SYSTEM_MANAGER_H

#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include "Infrastructure/Messaging/Messagers/MessageRouter.h"
#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messagers/MessageReplier.h"
#include "Infrastructure/Messaging/Messagers/MessagePusher.h"
#include "Infrastructure/Messaging/Messagers/MessagePuller.h"
#include "Infrastructure/Messaging/Messagers/MessagePublisher.h"
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
    // Messaging components for inter-agent communication
    std::shared_ptr<MessagerBase> task_control_router_;
    // Messaging component for receiving external commands from the host
    std::shared_ptr<MessagerBase> system_command_listener_;
    // Frame forwarding system
    std::shared_ptr<MessagerBase> data_collector;
    std::shared_ptr<MessagerBase> data_publisher_;

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
        const std::unique_ptr<MessageInterface> &message
    );
    void externalMessageHandler
    (
        const std::unique_ptr<MessageInterface> &message
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
    bool broadcastModeChange
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

    bool handleCalibrationCommand
    (
        const SystemCommandMsg &cmd_msg
    );

    bool handleConfigurationCommand
    (
        const SystemCommandMsg &cmd_msg
    );

    bool handleGetDataCommand
    (
        const SystemCommandMsg &cmd_msg
    );

    // Frame forwarding system
    void dataForwardingHandler
    (
        const std::unique_ptr<MessageInterface> &message
    );

    // Current system mode
    SystemMode_Type mode_ = SystemMode_Type::STANDBY;
};
} // namespace PiTrac

#endif // ENHANCED_SYSTEM_MANAGER_H