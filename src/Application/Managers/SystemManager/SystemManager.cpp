#include "Application/Managers/SystemManager/SystemManager.h"
#include "Infrastructure/Messaging/Messages/GSChangeModeMsg.h"

namespace PiTrac
{

SystemManager::SystemManager()
    : GSManagerBase("SystemManager", ManagerPriority::Critical)
    , mode_command_messager_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Reply))
    , command_handler_(nullptr)
{
}
SystemManager::~SystemManager()
{
    mode_command_messager_.reset();
    cleanup();
}

bool SystemManager::setup()
{
    command_handler_ = [this](std::unique_ptr<GSMessageInterface> message) {
        handleCommand(std::move(message));
    };
    mode_command_messager_->bind("tcp://0.0.0.0:6000");
    logInfo("SystemManager bound to tcp://0.0.0.0:6000 for mode change commands");
    // Setup the messager to subscribe to mode change commands
    mode_command_messager_->startReceiving(command_handler_);
    return true;
}

bool SystemManager::initialize()
{
    // Initialization logic here
    return true;
}

void SystemManager::execute()
{
    // Execution logic here
    while(!should_stop_.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void SystemManager::cleanup()
{
    // Cleanup logic here
}

void SystemManager::handleCommand(std::unique_ptr<GSMessageInterface> message)
{
    // Process the command message
    logInfo("Received command message: " + message->toString());
    const GSMessageType type = message->getMessageType();
    switch(type)
    {
        case GSMessageType::ChangeMode:
        {
            auto mode_msg = dynamic_cast<GSChangeModeMsg*>(message.get());
            if(mode_msg)
            {
                handleModeChange(mode_msg->getNewMode());
                mode_command_messager_->sendMessage(*message); // Echo back as acknowledgment
            }
            break;
        }
        default:
            logWarning("Received unknown command message type: " + message->toString());
            break;
    }
}

void SystemManager::handleModeChange(SystemMode_Type new_mode)
{
    logInfo("Handling mode change to: " + std::to_string(static_cast<int>(new_mode)));
}

}