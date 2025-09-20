#ifndef GSAgent_H
#define GSAgent_H

#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include "Infrastructure/Messaging/Messages/MessageTypes.h"
#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/RegisterTaskMsg.h"
#include "Infrastructure/TaskProcess/GSTaskBase.h"
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
class GSAgentBase : public GSTaskBase
{
  public:
    GSAgentBase
    (
        const std::string &name
    )
        : GSTaskBase(name)
        , agent_control_subscriber_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber))
        , agent_control_endpoint_(Endpoints::getAgentTaskEndpoint())
        , lm_mode_(SystemMode_Type::MAX_MODE)
    {
        message_handler_ = [this](std::unique_ptr<MessageInterface> message) {
                               this->messageHandler(std::move(message));
                           };
        logInfo("Agent created: " + name_ + " [" + task_id_ + "]");
    }

    ~GSAgentBase()
    {
        if (agent_thread_.joinable())
        {
            end();
            agent_thread_.join();
        }
        logInfo("Agent destroyed: " + name_);
    }

  protected:

    std::thread agent_thread_;
    std::unique_ptr<GSMessagerBase> agent_control_subscriber_;
    std::unique_ptr<GSMessagerBase> task_status_publisher_;
    std::string agent_control_endpoint_;
    std::function<void(std::unique_ptr<MessageInterface>)> message_handler_;
    PiTrac::SystemMode_Type lm_mode_;
    std::atomic<bool> run_;
    std::mutex mode_mutex_;

    // Override processMain, calls execute in a new thread. This will be
    // the main entry point for the agent.
    void processMain() override
    {
        if (getStatus() == TaskStatus::Running)
        {
            logWarning("Agent already running: " + name_);
            return;
        }

        if (getStatus() == TaskStatus::Paused)
        {
            changeStatus(TaskStatus::Running);
            logInfo("Resuming agent: " + name_);
            return;
        }
        GSMessagerBase task_reg_messager(GSMessagerBase::SocketType::Request);
        task_reg_messager.connect(Endpoints::getTaskRegistrationEndpoint());
        RegisterTaskMsg reg_msg(getpid(), name_);
        task_reg_messager.sendMessage(reg_msg);
        auto reply = task_reg_messager.receiveMessage<RegisterTaskMsg>(5000);
        if(reply)
        {
            logInfo("Agent registered with SystemManager: " + reply->toString());
        }
        else
        {
            logWarning("No reply from SystemManager on agent registration");
        }
        execute();
    }

    // The primary execution loop for an agent is to listen for control messages
    // and handle them accordingly.
    void execute()
    {
        logInfo("Starting agent: " + name_);
        changeStatus(TaskStatus::Running);
        run_.store(true);
        // Connect to the control endpoint
        agent_control_subscriber_->connect(agent_control_endpoint_);
        agent_control_subscriber_->startReceiving(message_handler_);
        logInfo("Agent connected to control endpoint: " + agent_control_endpoint_);

        // Main loop
        while (!should_stop_.load())
        {
            // TODO: Add periodic agent task logic... perhaps health checks,
            // status updates, etc.
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            logInfo("Agent " + name_ + " is running in mode: " + System::systemModeToString(lm_mode_));
        }

        // Cleanup
        agent_control_subscriber_->stop();
        logInfo("Agent has stopped: " + name_);
    }

    void messageHandler(std::unique_ptr<MessageInterface> message)
    {
        logInfo("Received message: " + message->toString());
        // Handle messages here, e.g., mode change commands
        const Message_Type type = message->getMessageType();
        switch(type)
        {
            case Message_Type::ChangeMode:
            {
                auto mode_msg = dynamic_cast<ChangeModeMsg *>(message.get());
                if(mode_msg)
                {
                    std::lock_guard<std::mutex> lock(mode_mutex_);
                    lm_mode_ = mode_msg->getNewMode();
                    logInfo("Mode changed to: " + std::to_string(static_cast<int>(lm_mode_)));
                    changeMode(lm_mode_);
                }
                break;
            }
            // TODO: Handle events
            // Case Message_Type::Event:
            //     handleEvent(dynamic_cast<GSEventMsg*>(message.get()));
            //     break;
            default:
                logWarning("Unknown message type received: " + message->toString());
                break;
        }
    }

    virtual void changeMode
    (
        PiTrac::SystemMode_Type new_mode
    ) = 0;
    // virtual void handleEvent(/*GSEventMsg* event_msg*/) = 0;
};
} // namespace PiTrac

#endif // GSAgent_H