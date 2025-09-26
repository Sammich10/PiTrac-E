#include "Application/Managers/SystemManager/SystemManager.h"

namespace PiTrac
{
SystemManager::SystemManager()
    : GSManagerBase("SystemManager")
    , task_reg_receiver_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Reply))
    , system_command_listener_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Reply))
    , task_status_subscriber_(std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Subscriber))
{
}

SystemManager::~SystemManager()
{
    task_reg_receiver_.reset();
}

bool SystemManager::setupProcess()
{
    task_reg_receiver_->bind(Endpoints::getTaskRegistrationEndpoint());
    task_reg_receiver_->startReceiving(
        std::bind(&SystemManager::taskRegistrationHandler, this, std::placeholders::_1)
        );
    system_command_listener_->bind(Endpoints::getExternalCommandEndpoint());
    system_command_listener_->startReceiving(
        std::bind(&SystemManager::handleExternalCommand, this, std::placeholders::_1)
        );
    task_status_subscriber_->connect(Endpoints::getTaskStatusEndpoint());
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
    task_reg_receiver_->stop();
    task_reg_receiver_.reset();
    logInfo("Stopping task status subscriber and command listener");
    system_command_listener_->stop();
    system_command_listener_.reset();
    logInfo("Stopping task status subscriber");
    task_status_subscriber_->stop();
    task_status_subscriber_.reset();
}

void SystemManager::handleExternalCommand(std::unique_ptr<MessageInterface> message)
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
}

void SystemManager::taskRegistrationHandler(std::unique_ptr<MessageInterface> message)
{
    logInfo("Received task registration message: " + message->toString());
    if(message->getMessageType() != Message_Type::RegisterTask)
    {
        logWarning("Received unexpected message type in task registration handler: " + std::to_string(static_cast<int>(message->getMessageType())));
        return;
    }
    auto reg_msg = dynamic_cast<RegisterTaskMsg *>(message.get());
    if(!reg_msg)
    {
        logError("Failed to cast message to RegisterTaskMsg in task registration handler");
        return;
    }
    for(const RegisteredTask &task : registered_tasks_)
    {
        if(task.task_pid == reg_msg->getTaskPid() || task.task_name == reg_msg->getTaskName())
        {
            logInfo("Task already registered: " + task.task_name + " [" + std::to_string(task.task_pid) + "]");
            return;
        }
    }
    RegisteredTask new_task;
    new_task.task_name = reg_msg->getTaskName();
    new_task.task_pid = reg_msg->getTaskPid();
    registered_tasks_.push_back(new_task);
    task_reg_receiver_->sendMessage(*reg_msg); // Echo back the registration
                                               // message as confirmation
    logInfo("Registered new task: " + new_task.task_name + " [" + std::to_string(new_task.task_pid) + "]");
}
} // namespace PiTrac