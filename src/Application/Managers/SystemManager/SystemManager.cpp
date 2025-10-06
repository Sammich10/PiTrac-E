#include "Application/Managers/SystemManager/SystemManager.h"

namespace PiTrac
{
SystemManager::SystemManager()
    : GSManagerBase("SystemManager")
    , task_control_router_(std::make_unique<MessageRouter>())
    , system_command_listener_(std::make_unique<MessagerBase>(MessagerBase::SocketType::Reply))
{
}

SystemManager::~SystemManager()
{
    task_control_router_.reset();
}

bool SystemManager::setupProcess()
{
    logInfo("Setting up task control router");
    task_control_router_->bind(Endpoints::getTaskControlEndpoint());
    task_control_router_->startReceivingWithIdentity(
        std::bind(&SystemManager::taskControlMessageHandler, this, std::placeholders::_1)
        );
    logInfo("Setting up system command listener");
    system_command_listener_->bind(Endpoints::getExternalCommandEndpoint());
    system_command_listener_->startReceiving(
        std::bind(&SystemManager::externalMessageHandler, this, std::placeholders::_1)
        );
    return true;
}

bool SystemManager::execute()
{
    changeStatus(TaskStatus::Running);
    logInfo("SystemManager is running");
    while(!should_stop_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

void SystemManager::cleanupProcess()
{
    logInfo("Stopping task registration and command listeners");
    task_control_router_->stop();
    task_control_router_.reset();
    logInfo("Stopping system command listener");
    system_command_listener_->stop();
    system_command_listener_.reset();
}

void SystemManager::externalMessageHandler(std::unique_ptr<MessageInterface> message)
{
    logInfo("Received external command: " + message->toString());
    if(message->getMessageType() == Message_Type::SystemCommand)
    {
        logInfo("Handling SystemCommand message");
        auto cmd_msg = dynamic_cast<SystemCommandMsg *>(message.get());
        if(cmd_msg)
        {
            switch(cmd_msg->getCommandID())
            {
                case SystemCommandMsg::CommandID::SetMode:
                    logInfo("Handling SetMode command");
                    SystemCommandMsg::SetModePayload mode_change_payload_;
                    if(!extractCommandPayload<SystemCommandMsg::SetModePayload>(*cmd_msg, mode_change_payload_))
                    {
                        logError("Failed to extract SetModePayload from SystemCommandMsg");
                        break;
                    }
                    handleModeChangeCommand(mode_change_payload_);
                    break;
                default:
                    logWarning("Received unknown command ID in SystemCommandMsg: " + std::to_string(static_cast<int>(cmd_msg->getCommandID())));
                    break;
            }
        }
        else
        {
            logError("Failed to cast message to SystemCommandMsg in external command handler");
        }
        AckMessage ack_msg(std::move(message), AckMessage::Status::Success);
        system_command_listener_->sendMessage(ack_msg);
    }
    else
    {
        logWarning("Received unexpected message type in external command handler: " + std::to_string(static_cast<int>(message->getMessageType())));
        AckMessage ack_msg(std::move(message), AckMessage::Status::Failure);
        system_command_listener_->sendMessage(ack_msg);
    }
}

template<typename T>
bool SystemManager::extractCommandPayload(const SystemCommandMsg &msg, T &payload) const
{
    try
    {
        payload = std::get<T>(msg.getPayload());
        return true;
    }
    catch (const std::bad_variant_access &e)
    {
        logError("Failed to extract command payload from SystemCommandMsg: " + std::string(e.what()));
        return false;
    }
}

void SystemManager::handleModeChangeCommand(const SystemCommandMsg::SetModePayload &payload)
{
    logInfo("Changing system mode to: " + std::to_string(static_cast<int>(payload.mode)));
    broadcastModeChange(payload.mode);
    mode_ = payload.mode;
}

// Enhanced handler for ROUTER-DEALER pattern with identity
void SystemManager::taskControlMessageHandler(std::unique_ptr<MessagerBase::IdentityMessage> identity_message)
{
    const std::string &sender_identity = identity_message->sender_identity;
    std::unique_ptr<MessageInterface> &message = identity_message->message;

    const Message_Type type = message->getMessageType();
    switch(type)
    {
        case Message_Type::RegisterTask:
        {
            handleAgentRegistration(sender_identity, *dynamic_cast<RegisterTaskMsg *>(message.get()));
            // Send initial mode to the newly registered agent to synchronize its state with the system
            sendModeChangeToAgent(sender_identity, mode_);
            break;
        }
        case Message_Type::Heartbeat:
        {
            handleAgentHeartbeat(sender_identity, *dynamic_cast<HeartbeatMsg *>(message.get()));
            break;
        }
        default:
            logWarning("Received unknown message type from identity [" + sender_identity + "]: " +
                       std::to_string(static_cast<int>(type)));
            sendAcknowledgmentToAgent(sender_identity, *message, false);
            break;
    }
}

void SystemManager::handleAgentRegistration(const std::string &identity, const RegisterTaskMsg &reg_msg)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);

    // Check if identity already exists
    {
        std::lock_guard<std::mutex> router_lock(router_mutex_);
        if (registered_agents_.find(identity) != registered_agents_.end())
        {
            logWarning("Identity [" + identity + "] is already registered, replacing previous registration");
        }
        RegisteredAgent agent(reg_msg.getTaskName(), reg_msg.getTaskPid(), identity);
        registered_agents_[identity] = agent;
        agent_name_to_identity_[reg_msg.getTaskName()] = identity;
    }

    logInfo("Agent registered: " + reg_msg.getTaskName() +
            " (PID: " + std::to_string(reg_msg.getTaskPid()) +
            ", Identity: " + identity + ")");

    // Log all currently registered agents
    logInfo("Total registered agents: " + std::to_string(registered_agents_.size()));
    for (const auto &pair : registered_agents_)
    {
        logInfo("  - Identity: " + pair.first + ", Name: " + pair.second.task_name +
                ", PID: " + std::to_string(pair.second.task_pid));
    }

    logInfo("Acknowledging registration to agent: " + reg_msg.getTaskName());
    // Send acknowledgment back to agent
    try {
        // Create a simple ack message (without embedding the original message)
        AckMessage ack_msg(AckMessage::Status::Success);
        logInfo("Sending AckMessage to identity: " + identity);

        // Protect router socket access from concurrent async handlers
        {
            std::lock_guard<std::mutex> router_lock(router_mutex_);
            task_control_router_->sendMessageToIdentity(ack_msg, identity);
        }

        logInfo("AckMessage sent successfully to: " + identity);
    } catch (const std::exception &e) {
        logError("Failed to send registration ack to " + identity + ": " + std::string(e.what()));
    }

    logInfo("Registration complete for agent: " + reg_msg.getTaskName());
}

void SystemManager::handleAgentHeartbeat(const std::string &identity, const HeartbeatMsg &heartbeat)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto it = registered_agents_.find(identity);
    if(it != registered_agents_.end())
    {
        it->second.last_seen = std::chrono::system_clock::now();
        logInfo("Heartbeat received from: [" + it->second.task_name + "] " + 
                "PID: [" + std::to_string(heartbeat.getPid()) + "]" + 
                ", Status: [" + taskStatusToString(heartbeat.getStatus()) + "]" +
                ", Mode: [" + System::systemModeToString(heartbeat.getMode()) + "]");
    }
    else
    {
        logWarning("Received heartbeat from unregistered identity: " + identity);
    }
}

void SystemManager::sendAcknowledgmentToAgent(const std::string &identity, const MessageInterface &original_message, bool success)
{
    AckMessage ack_msg(std::move(original_message.clone()),
                       success ? AckMessage::Status::Success : AckMessage::Status::Failure);
    try {
        std::lock_guard<std::mutex> router_lock(router_mutex_);
        task_control_router_->sendMessageToIdentity(ack_msg, identity);
    } catch (const std::exception &e) {
        logError("Failed to send acknowledgment to " + identity + ": " + std::string(e.what()));
    }
}

void SystemManager::sendModeChangeToAgent(const std::string &identity, SystemMode_Type new_mode)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto agent_it = registered_agents_.find(identity);
    if(agent_it != registered_agents_.end())
    {
        ChangeModeMsg mode_msg(new_mode);
        try {
            std::lock_guard<std::mutex> router_lock(router_mutex_);
            task_control_router_->sendMessageToIdentity(mode_msg, identity);
            agent_it->second.current_mode = new_mode;
            logInfo("Sent mode change to " + agent_it->second.task_name + " (Identity: " + identity + ") to mode " + std::to_string(static_cast<int>(new_mode)));
        } catch (const std::exception &e) {
            logError("Failed to send mode change to " + agent_it->second.task_name + ": " + std::string(e.what()));
        }
    }
    else
    {
        logWarning("Agent identity not found in registered agents: " + identity);
    }

}

void SystemManager::broadcastModeChange(SystemMode_Type new_mode)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);

    for(const auto & [identity, agent] : registered_agents_)
    {
        ChangeModeMsg mode_msg(new_mode);
        try {
            std::lock_guard<std::mutex> router_lock(router_mutex_);
            task_control_router_->sendMessageToIdentity(mode_msg, identity);
            registered_agents_[identity].current_mode = new_mode;
            logInfo("Broadcasted mode change to " + agent.task_name + " (Identity: " + identity + ") to mode " + std::to_string(static_cast<int>(new_mode)));
        } catch (const std::exception &e) {
            logError("Failed to broadcast mode change to " + agent.task_name + ": " + std::string(e.what()));
        }
    }
}

void SystemManager::checkAgentTimeouts()
{
    std::lock_guard<std::mutex> lock(agents_mutex_);
    auto now = std::chrono::system_clock::now();
    for(auto it = registered_agents_.begin(); it != registered_agents_.end(); )
    {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_seen).count();
        if(duration > 10) // 10 seconds timeout
        {
            logWarning("Agent timed out: " + it->second.task_name + " (Identity: " + it->first + ")");
            agent_name_to_identity_.erase(it->second.task_name);
            it = registered_agents_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

std::vector<SystemManager::RegisteredAgent> SystemManager::getActiveAgents()
{
    std::lock_guard<std::mutex> lock(agents_mutex_);
    std::vector<RegisteredAgent> agents;

    for(const auto & [identity, agent] : registered_agents_)
    {
        agents.push_back(agent);
    }

    return agents;
}
} // namespace PiTrac