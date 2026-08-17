#include "Application/Managers/SystemManager/SystemManager.h"
#include "Common/Utils/Calibration/CalibrationData.h"

namespace PiTrac
{
SystemManager::SystemManager()
    : GSManagerBase("SystemManager")
    , task_control_router_(std::make_unique<MessageRouter>())
    , system_command_listener_(std::make_unique<MessagerBase>(MessagerBase::SocketType::Reply))
    , data_collector(std::make_unique<MessagerBase>(MessagerBase::SocketType::Pull))
    , data_publisher_(std::make_unique<MessagerBase>(MessagerBase::SocketType::Publisher))
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
    task_control_router_->startReceiving(
        std::bind(&SystemManager::taskControlMessageHandler, this, std::placeholders::_1)
        );
    logInfo("Setting up system command listener");
    system_command_listener_->bind(Endpoints::getExternalCommandEndpoint());
    system_command_listener_->startReceiving(
        std::bind(&SystemManager::externalMessageHandler, this, std::placeholders::_1)
        );

    logInfo("Setting up frame forwarding system");
    data_collector->bind(Endpoints::getDataCollectionEndpoint());
    data_collector->startReceiving(
        std::bind(&SystemManager::dataForwardingHandler, this, std::placeholders::_1)
        );
    data_publisher_->bind(Endpoints::getOutgoingDataEndpoint());

    // For now, we can create the calibration database here to ensure the
    // database is set up before any agents try to access it
    // This should only need to happen once on first run, and the database file
    // will persist across runs, so it won't cause overhead on subsequent runs
    std::string err;
    if(!CalibrationData::createDatabaseIfNotExists(err))
    {
        logError("Failed to initialize calibration database: " + err);
        return false;
    }

    return true;
}

bool SystemManager::execute()
{
    changeStatus(TaskStatus::Running);
    logInfo("SystemManager is running");
    while(!should_stop_)
    {   // TODO: Publish system status updates here as well, including active
        // agents and their modes
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

    logInfo("Stopping frame forwarding system");
    data_collector->stop();
    data_publisher_->stop();
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
            SystemCommandMsg::CommandID command_id = static_cast<SystemCommandMsg::CommandID>(cmd_msg->getCommand_id());
            switch(command_id)
            {
                case SystemCommandMsg::CommandID::SetMode:
                {
                    logInfo("Handling SetMode command");
                    const bool modeChangeSuccess = handleModeChangeCommand(*cmd_msg);
                    sendAcknowledgementToHost(*message, modeChangeSuccess);
                    break;
                }
                case SystemCommandMsg::CommandID::Calibrate:
                {
                    logInfo("Handling Calibrate command");
                    if(mode_ != SystemMode_Type::CALIBRATION)
                    {
                        logWarning("Received Calibrate command while not in CALIBRATION mode");
                        sendAcknowledgementToHost(*message, false);
                        break;
                    }
                    // Forward the calibration command to specified agent(s)
                    const bool commandSuccess = handleCalibrationCommand(*cmd_msg);
                    sendAcknowledgementToHost(*message, commandSuccess);
                    break;
                }
                case SystemCommandMsg::CommandID::Configure:
                {
                    logInfo("Handling Configure command");
                    // This command can be handled by the SystemManager itself
                    // or forwarded to agents based on parameters
                    const bool commandSuccess = handleConfigurationCommand(*cmd_msg);
                    sendAcknowledgementToHost(*message, commandSuccess);
                    break;
                }
                case SystemCommandMsg::CommandID::GetData:
                {
                    logInfo("Handling GetData command");
                    // This command can be handled by the SystemManager itself
                    // or forwarded to agents based on parameters
                    // const bool commandSuccess =
                    // handleGetDataCommand(*cmd_msg);
                    sendAcknowledgementToHost(*message, false);
                    break;
                }
                default:
                    logWarning("Received unknown command ID in SystemCommandMsg: " + std::to_string(static_cast<int>(cmd_msg->getCommand_id())));
                    sendAcknowledgementToHost(*message, false);
                    break;
            }
        }
        else
        {
            sendAcknowledgementToHost(*message, false);
            logError("Failed to cast message to SystemCommandMsg in external command handler");
        }
    }
    else
    {
        logWarning("Received unexpected message type in external command handler: " + std::to_string(static_cast<int>(message->getMessageType())));
    }
}

bool SystemManager::handleModeChangeCommand(const SystemCommandMsg &cmd_msg)
{
    std::map<std::string, std::string> params = cmd_msg.getCommand_params();
    std::string mode_str = "";
    auto it = params.find("mode");
    if(it == params.end())
    {
        logError("SetMode command missing 'mode' parameter");
        logError("Received message: " + cmd_msg.toString());
        return false;
    }
    else
    {
        mode_str = it->second;
    }
    // Convert to uppercase
    std::transform(mode_str.begin(), mode_str.end(), mode_str.begin(),
                   [](unsigned char c) {
            return std::toupper(c);
        });
    SystemMode_Type new_mode = System::stringToSystemMode(mode_str);
    if(new_mode == SystemMode_Type::MAX_MODE)
    {
        logError("SetMode command received invalid mode string: " + mode_str);
        return false;
    }
    {
        if(new_mode != mode_)
        {
            logInfo("Changing system mode from " + System::systemModeToString(mode_) +
                    " to " + System::systemModeToString(new_mode));
            mode_ = new_mode;
            broadcastModeChange(new_mode);
        }
        else
        {
            logInfo("System already in requested mode: " + System::systemModeToString(new_mode));
        }
    }
    return true;
}

bool SystemManager::handleCalibrationCommand(const SystemCommandMsg &cmd_msg)
{
    logInfo("Forwarding calibration command to registered agents in CALIBRATION mode");
    std::map<std::string, std::string> params = cmd_msg.getCommand_params();

    std::lock_guard<std::mutex> lock(agents_mutex_);
    if(params.find("task_name") == params.end())
    {
        // Broadcast to all agents in CALIBRATION mode
        for(const auto &pair : registered_agents_)
        {
            const RegisteredAgent &agent = pair.second;
            if(agent.current_mode == SystemMode_Type::CALIBRATION)
            {
                logInfo("Sending calibration command to agent: " + agent.task_name);
                try
                {
                    SystemCommandMsg calib_msg;
                    calib_msg.setCommand_id(static_cast<int32_t>(SystemCommandMsg::CommandID::Calibrate));
                    calib_msg.setCommand_params(params);
                    task_control_router_->sendMessage(calib_msg, agent.zmq_identity);
                }
                catch(const std::exception &e)
                {
                    logError("Failed to send calibration command to agent " + agent.task_name + ": " + std::string(e.what()));
                }
            }
        }
    }
    else
    {
        // Send to specific agent
        std::string target_agent_name = params["task_name"];
        auto it = agent_name_to_identity_.find(target_agent_name);
        if(it != agent_name_to_identity_.end())
        {
            std::string identity = it->second;
            logInfo("Sending calibration command to specified agent: " + target_agent_name);
            try
            {
                // Remove the task_name parameter before forwarding
                params.erase("task_name");
                SystemCommandMsg calib_msg;
                calib_msg.setCommand_id(static_cast<int32_t>(SystemCommandMsg::CommandID::Calibrate));
                calib_msg.setCommand_params(params);
                task_control_router_->sendMessage(calib_msg, identity);
            }
            catch(const std::exception &e)
            {
                logError("Failed to send calibration command to agent " + target_agent_name + ": " + std::string(e.what()));
            }
        }
        else
        {
            logError("Specified agent for calibration command not found: " + target_agent_name);
            return false;
        }
    }
    return true;
}

bool SystemManager::handleConfigurationCommand(const SystemCommandMsg &cmd_msg)
{
    std::map<std::string, std::string> params = cmd_msg.getCommand_params();

    std::lock_guard<std::mutex> lock(agents_mutex_);
    if(params.find("task_name") == params.end())
    {   // For now assume a configuration command must have a target agent,
        // but in the future we could allow for some system-level configuration
        // commands that don't require a target
        logError("Configure command missing 'task_name' parameter");
        return false;
    }
    std::string target_agent_name = params["task_name"];
    // Remove task_name from params before forwarding, as it's only used for routing and not needed by the agent itself
    params.erase("task_name");
    if(target_agent_name.empty())
    {
        logError("Configure command has empty 'task_name' parameter");
        return false;
    }
    if(target_agent_name == "system")
    {
        logInfo("Received configuration command targeting the system itself");
        // Handle any system-level configuration commands here based on other parameters
        // For now, we don't have any specific system-level configurations, so just log and return success
        logInfo("No specific system-level configuration handling implemented yet");
        return true;
    }
    auto it = agent_name_to_identity_.find(target_agent_name);
    if(it != agent_name_to_identity_.end())
    {
        std::string identity = it->second;
        logInfo("Sending configuration command to specified agent: " + target_agent_name);
        try
        {
            // Remove the task_name parameter before forwarding
            params.erase("task_name");
            SystemCommandMsg config_msg;
            config_msg.setCommand_id(static_cast<int32_t>(SystemCommandMsg::CommandID::Configure));
            config_msg.setCommand_params(params);
            task_control_router_->sendMessage(config_msg, identity);
        }
        catch(const std::exception &e)
        {
            logError("Failed to send configuration command to agent " + target_agent_name + ": " + std::string(e.what()));
            return false;
        }
    }
    else
    {
        logError("Specified agent for configuration command not found: " + target_agent_name);
        return false;
    }
    return true;
}

// Enhanced handler for ROUTER-DEALER pattern with identity
void SystemManager::taskControlMessageHandler(std::unique_ptr<MessageInterface> message)
{
    const std::string &sender_identity = message->getIdentity();

    const Message_Type type = message->getMessageType();
    switch(type)
    {
        case Message_Type::RegisterTask:
        {
            handleAgentRegistration(sender_identity, *dynamic_cast<RegisterTaskMsg *>(message.get()));
            // Send initial mode to the newly registered agent to synchronize
            // its state with the system
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
        sendAcknowledgmentToAgent(identity, reg_msg, true);
        logInfo("Ack message sent successfully to: " + identity);
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
                ", Status: [" + taskStatusToString(static_cast<TaskStatus>(heartbeat.getTask_status())) + "]" +
                ", Mode: [" + System::systemModeToString(static_cast<SystemMode_Type>(heartbeat.getSystem_mode())) + "]");
    }
    else
    {
        logWarning("Received heartbeat from unregistered identity: " + identity);
    }
}

void SystemManager::sendAcknowledgmentToAgent(const std::string &identity, const MessageInterface &original_message, const bool success)
{
    AckMessage ack(
        static_cast<int32_t>(success ? AckMessage::AckStatus::Success : AckMessage::AckStatus::Failure),
        static_cast<int32_t>(original_message.getMessageType()),
        original_message.getTimestamp().time_since_epoch().count()
        );
    try
    {
        std::lock_guard<std::mutex> router_lock(router_mutex_);
        task_control_router_->sendMessage(ack, identity);
        logInfo("Sent acknowledgment to agent: " + identity);
    }
    catch (const std::exception &e)
    {
        logError("Failed to send acknowledgment to " + identity + ": " + std::string(e.what()));
    }
}

void SystemManager::sendAcknowledgementToHost(const MessageInterface &original_message, const bool success)
{
    AckMessage ack(
        static_cast<int32_t>(success ? AckMessage::AckStatus::Success : AckMessage::AckStatus::Failure),
        static_cast<int32_t>(original_message.getMessageType()),
        original_message.getTimestamp().time_since_epoch().count()
        );
    try
    {
        system_command_listener_->sendMessage(ack);
        logInfo("Sent acknowledgment to host for message type " + std::to_string(static_cast<int>(original_message.getMessageType())));
    }
    catch (const std::exception &e)
    {
        logError("Failed to send acknowledgment to host: " + std::string(e.what()));
    }
}

void SystemManager::sendModeChangeToAgent(const std::string &identity, SystemMode_Type new_mode)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);

    auto agent_it = registered_agents_.find(identity);
    if(agent_it != registered_agents_.end())
    {
        ChangeModeMsg mode_msg((int32_t)new_mode);
        try {
            std::lock_guard<std::mutex> router_lock(router_mutex_);
            task_control_router_->sendMessage(mode_msg, identity);
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

bool SystemManager::broadcastModeChange(SystemMode_Type new_mode)
{
    std::lock_guard<std::mutex> lock(agents_mutex_);
    // Send the mode change to all registered agents
    for(const auto & [identity, agent] : registered_agents_)
    {
        ChangeModeMsg mode_msg((int32_t)new_mode);
        try {
            std::lock_guard<std::mutex> router_lock(router_mutex_);
            task_control_router_->sendMessage(mode_msg, identity);
            registered_agents_[identity].current_mode = new_mode;
            logInfo("Broadcasted mode change to " + agent.task_name + " (Identity: " + identity + ") to mode " + std::to_string(static_cast<int>(new_mode)));
        } catch (const std::exception &e) {
            logError("Failed to broadcast mode change to " + agent.task_name + ": " + std::string(e.what()));
            return false;
        }
    }
    return true;
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

void SystemManager::dataForwardingHandler(std::unique_ptr<MessageInterface> message)
{
    // Simply forward any received frame data to Flask
    if (message)
    {
        try
        {
            data_publisher_->sendMessage(*message);
        }
        catch (const std::exception &e)
        {
            logError("Failed to forward frame: " + std::string(e.what()));
        }
    }
}
} // namespace PiTrac