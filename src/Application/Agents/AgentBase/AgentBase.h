#ifndef GSAgent_H
#define GSAgent_H

#include "Infrastructure/Messaging/Messagers/MessageDealer.h"
#include "Infrastructure/Messaging/Messages/MessageTypes.h"
#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/RegisterTaskMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/HeartbeatMsg.h"
#include "Infrastructure/Messaging/Messages/Common/AckMessage.h"
#include "Infrastructure/TaskProcess/TaskBase.h"
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
class AgentBase : public TaskBase
{
  public:
    AgentBase
    (
        const std::string &name
    )
        : TaskBase(name)
        , agent_control_(std::make_unique<MessageDealer>(name + "_" + std::to_string(getpid())))
        , agent_control_endpoint_(Endpoints::getTaskControlEndpoint())
        , lm_mode_(SystemMode_Type::MAX_MODE)
    {
        message_handler_ = [this](std::unique_ptr<MessageInterface> message) {
                               this->messageHandler(std::move(message));
                           };
        logInfo("Agent created: " + name_ + " [" + task_id_ + "]");
    }

    ~AgentBase()
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
    std::unique_ptr<MessageDealer> agent_control_;
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
        // Use DEALER socket for registration (part of ROUTER-DEALER pattern)
        // The agent_control_ is already a DEALER socket, so we'll use that for registration too
        auto dealer_identity = agent_control_->getIdentity();
        if (dealer_identity.has_value()) {
            logInfo("DEALER socket identity: " + dealer_identity.value());
        } else {
            logWarning("DEALER socket has no identity set!");
        }
        agent_control_->connect(agent_control_endpoint_);
        
        RegisterTaskMsg reg_msg(getpid(), name_);
        int registration_timeout = 10000; // 10 seconds
        const int registration_message_interval = 1000; // 1 second between retries
        while(registration_timeout > 0)
        {
            logInfo("Registering agent with SystemManager via DEALER socket: " + name_);
            try
            {
                logInfo("Calling sendRequestAndWaitForResponse...");
                std::unique_ptr<MessageInterface> response = agent_control_->sendRequestAndWaitForResponse(reg_msg, registration_message_interval); // 1 second timeout
                logInfo("sendRequestAndWaitForResponse returned, checking response...");
                if (response)
                {
                    logInfo("Received response to registration: " + response->toString());
                    if(response->getMessageType() != Message_Type::AckMessage)
                    {
                        logError("Unexpected response type during registration: " + response->toString());
                        continue; // Retry
                    }
                    auto ack_msg = dynamic_cast<AckMessage*>(response.get());
                    if(ack_msg->getStatus() != AckMessage::Status::Success)
                    {
                        logError("Registration failed, received NACK: " + ack_msg->toString());
                        continue; // Retry
                    }
                    logInfo("Registration acknowledged by SystemManager: " + response->toString());
                    break; // Successfully registered
                }
                else
                {
                    logError("No response received for registration attempt, retrying...");
                }
            }
            catch (const std::exception &e)
            {
                logError("Registration attempt failed: " + std::string(e.what()));
            }
            registration_timeout -= registration_message_interval;
            logInfo("Retrying registration... Time left: " + std::to_string(registration_timeout) + " milliseconds");
        }
        if (registration_timeout <= 0)
        {
            logError("Failed to register with SystemManager after multiple attempts. Exiting.");
            exit(1);
        }
        
        // Set up message handler before starting to receive
        agent_control_->startReceiving(message_handler_);
        
        execute();
    }

    // The primary execution loop for an agent is to listen for control messages
    // and handle them accordingly.
    void execute()
    {
        logInfo("Starting agent: " + name_);
        changeStatus(TaskStatus::Running);
        run_.store(true);
        
        logInfo("Agent connected to SystemManager via ROUTER-DEALER pattern: " + agent_control_endpoint_);

        // Main loop - send periodic heartbeats and handle incoming commands
        while (!should_stop_.load())
        {
            // Send heartbeat to SystemManager
            sendHeartbeat();
            
            // TODO: Add periodic agent task logic... perhaps health checks,
            // status updates, etc.
            std::this_thread::sleep_for(std::chrono::milliseconds(5000)); // 5 second heartbeat
            
            if (lm_mode_ != SystemMode_Type::MAX_MODE) {
                // logInfo("Agent " + name_ + " is running in mode: " + System::systemModeToString(lm_mode_));
            }
        }

        // Send shutdown notification to SystemManager
        sendShutdownNotification();
        
        // Cleanup
        agent_control_->stop();
        logInfo("Agent has stopped: " + name_);
    }

    void messageHandler(std::unique_ptr<MessageInterface> message)
    {
        logInfo("Received message: " + message->toString());
        
        // Handle messages here and send acknowledgments back to SystemManager
        const Message_Type type = message->getMessageType();
        bool command_success = false;
        
        switch(type)
        {
            case Message_Type::AckMessage:
            {
                // Handle registration acknowledgment from SystemManager
                auto ack_msg = dynamic_cast<AckMessage *>(message.get());
                if(ack_msg)
                {
                    logInfo("Acknowledgment received from SystemManager: " + ack_msg->toString());
                    command_success = true;
                }
                break;
            }
            case Message_Type::ChangeMode:
            {
                auto mode_msg = dynamic_cast<ChangeModeMsg *>(message.get());
                if(mode_msg)
                {
                    std::lock_guard<std::mutex> lock(mode_mutex_);
                    PiTrac::SystemMode_Type old_mode = lm_mode_;
                    lm_mode_ = mode_msg->getNewMode();
                    logInfo("Mode changed from " + System::systemModeToString(old_mode) + 
                           " to " + System::systemModeToString(lm_mode_));
                    changeMode(lm_mode_);
                    command_success = true;
                }
                break;
            }
            // TODO: Handle events
            // Case Message_Type::Event:
            //     handleEvent(dynamic_cast<GSEventMsg*>(message.get()));
            //     break;
            default:
                logWarning("Unknown message type received: " + message->toString());
                command_success = false;
                break;
        }
        
        // Send acknowledgment back to SystemManager (ROUTER-DEALER pattern)
        // sendCommandAcknowledgment(type, command_success);
    }

    virtual void changeMode
    (
        PiTrac::SystemMode_Type new_mode
    ) = 0;
    // virtual void handleEvent(/*GSEventMsg* event_msg*/) = 0;

    // ROUTER-DEALER pattern helper methods
    void sendHeartbeat()
    {
        HeartbeatMsg heartbeat(getpid(), name_, getStatus(), lm_mode_);
        try {
            agent_control_->sendMessage(heartbeat);
            logInfo("Heartbeat sent to SystemManager");
        } catch (const std::exception& e) {
            logError("Failed to send heartbeat: " + std::string(e.what()));
        }
    }
    
    void sendCommandAcknowledgment(Message_Type command_type, bool success, const std::string& error_msg = "")
    {
        // AckMessage ack(getpid(), name_, command_type, success, error_msg);
        // try {
        //     agent_control_->sendMessage(ack);
        //     std::string status = success ? "SUCCESS" : "FAILED";
        //     logInfo("Command acknowledgment sent: " + std::to_string(static_cast<int>(command_type)) + 
        //            " - " + status);
        // } catch (const std::exception& e) {
        //     logError("Failed to send command acknowledgment: " + std::string(e.what()));
        // }
    }
    
    void sendShutdownNotification()
    {
        // TODO: Create ShutdownMsg class
        // ShutdownMsg shutdown(getpid(), name_);
        // agent_control_->sendMessage(shutdown);
        
        // For now, use RegisterTaskMsg as shutdown notification (placeholder)
        try {
            RegisterTaskMsg shutdown(getpid(), name_ + "_shutdown");
            agent_control_->sendMessage(shutdown);
            logInfo("Shutdown notification sent to SystemManager");
        } catch (const std::exception& e) {
            logError("Failed to send shutdown notification: " + std::string(e.what()));
        }
    }
};

} // namespace PiTrac

#endif // GSAgent_H