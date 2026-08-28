#include "Foundation/AgentBase/AgentBase.h"

namespace PiTrac
{
AgentBase::AgentBase(const std::string &name)
    : TaskBase(name)
    , agent_control_(std::make_shared<MessageDealer>(name + "_" + std::to_string(getpid())))
    , lm_mode_(SystemMode_Type::MAX_MODE)
{
}

AgentBase::~AgentBase()
{
}

bool AgentBase::setupProcess()
{
    auto dealer_identity = agent_control_->getIdentity();
    if (dealer_identity.has_value())
    {
        logInfo("DEALER socket identity: " + dealer_identity.value());
    }
    else
    {
        logError("DEALER socket has no identity set!");
        return false;
    }

    std::unique_ptr<EventThread> agent_control_thread = std::make_unique<EventThread>(
        agent_control_,
        std::bind(&AgentBase::messageHandler, this, std::placeholders::_1)
        );

    agent_control_thread->connectEndpoint(Endpoints::getTaskControlEndpoint());

    event_threads_.push_back(std::move(agent_control_thread));

    // Start the event threads for the agent
    TaskBase::setupProcess();

    const bool registered = registerAgent();
    if(!registered)
    {
        logError("Failed to register agent with SystemManager");
        return false;
    }
    logInfo(getTaskName() + " connected to SystemManager");

    return registered;
}

void AgentBase::processMain()
{
    logInfo("Running agent: " + name_);
    // Default agent main loop - send periodic heartbeats
    while (!should_stop_.load())
    {
        // Send heartbeat to SystemManager
        sendHeartbeat();

        // TODO: Add periodic agent task logic... perhaps health checks,
        // status updates, etc.
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
    }
}

void AgentBase::cleanupProcess()
{
    TaskBase::cleanupProcess();
    // TODO: Create ShutdownMsg class
    // ShutdownMsg shutdown(getpid(), name_);
    // agent_control_->sendMessage(shutdown);

    // For now, use RegisterTaskMsg as shutdown notification (placeholder)
    RegisterTaskMsg shutdown(getpid(), name_ + "_shutdown");
    agent_control_->sendMessage(shutdown);
    logInfo("Shutdown notification sent to SystemManager");
}

void AgentBase::messageHandler(const std::unique_ptr<MessageInterface> &message)
{
    logInfo("Received message: " + message->toString());

    const Message_Type type = message->getMessageType();

    switch(type)
    {
        case Message_Type::AckMessage:
            // Handle acknowledgment message
            handleAcknowledgment(dynamic_cast<const AckMessage *>(message.get()));
            break;
        case Message_Type::ChangeMode:
            // Handle change mode message
            handleChangeMode(dynamic_cast<const ChangeModeMsg *>(message.get()));
            break;
        case Message_Type::SystemCommand:
            // Handle system command message
            handleSystemCommand(dynamic_cast<const SystemCommandMsg *>(message.get()));
            break;
        default:
            logWarning("Unhandled message type: " + std::to_string(static_cast<int>(type)));
            break;
    }
}

void AgentBase::sendHeartbeat()
{
    HeartbeatMsg heartbeat(getpid(), name_, (int32_t)getStatus(), (int32_t)lm_mode_);
    if(agent_control_->sendMessage(heartbeat) != MessagerBase::RequestStatus::Success)
    {
        logError(name_ + ": Failed to send heartbeat message");
    }
}

inline bool AgentBase::registerAgent()
{
    RegisterTaskMsg reg_msg(getpid(), name_);
    // 10 seconds
    int registration_timeout = 10000;
    // 1 second between retries
    constexpr int registration_message_interval = 1000;

    do
    {
        const MessagerBase::RequestStatus send_status = agent_control_->sendMessage(reg_msg);
        if(send_status != MessagerBase::RequestStatus::Success)
        {
            logError("Failed to send registration message: " + std::to_string(static_cast<int>(send_status)));
            return false;
        }

        // wait for registered_ flag to be set or timeout
        std::this_thread::sleep_for(std::chrono::milliseconds(registration_message_interval));
        registration_timeout -= registration_message_interval;
        if(registered_.load())
        {
            return true;
        }
        logInfo("Retrying registration... Time left: " + std::to_string(registration_timeout) + " milliseconds");
    }
    while (registration_timeout > 0);

    if (registration_timeout <= 0)
    {
        logError("Failed to register with SystemManager after multiple attempts. Exiting.");
        return false;
    }
    return true;
}
} // namespace PiTrac