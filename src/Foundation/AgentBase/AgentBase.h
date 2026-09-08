#ifndef GSAgent_H
#define GSAgent_H

#include "Infrastructure/Messaging/Messagers/MessageDealer.h"
#include "Infrastructure/Messaging/Messages/MessageTypes.h"
#include "Infrastructure/Messaging/Messages/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/RegisterTaskMsg.h"
#include "Infrastructure/Messaging/Messages/HeartbeatMsg.h"
#include "Infrastructure/Messaging/Messages/AckMessage.h"
#include "Foundation/TaskProcess/TaskBase.h"
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
    );

    ~AgentBase();

  protected:
    // Primary agent
    std::shared_ptr<MessageDealer> agent_control_;
    PiTrac::SystemMode_Type lm_mode_;

    /**
     * @brief Sets up the agent process, including connecting to the System Manager and registering the agent.
     *
     * @return true if setup was successful, false otherwise.
     */
    virtual bool setupProcess() override;

    /**
     * @brief Agent task main loop
     *
     * @note default behavior is to send periodic heartbeats to the System Manager. Can be overridden, but derived classes should
     * implement base class behavior to ensure heartbeats are sent.
     */
    virtual void processMain() override;

    /**
     * @brief Cleans up the agent process, including disconnecting from the System Manager and stopping any ongoing tasks.
     */
    virtual void cleanupProcess() override;

    /**
     * @brief Handling incoming commands via the agent control socket.
     *
     * @param[in] message The incoming message to be processed.
     *
     * @note Derived classes should call the base class implementation to ensure proper handling of standard messages.
     */
    virtual void messageHandler
    (
        const std::unique_ptr<MessageInterface> &message
    );

    /**
     * @brief Handles acknowledgment messages received by the agent through the agent control socket.
     *
     * @param[in] ack_msg The acknowledgment message to be processed.
     *
     * @return true if the acknowledgment was handled successfully, false otherwise.
     */
    virtual bool handleAcknowledgment
    (
        const AckMessage *ack_msg
    )
    {
        return true;
    };

    /**
     * @brief Handles change mode messages received by the agent through the agent control socket.
     *
     * @param[in] mode_msg The change mode message to be processed.
     *
     * @return true if the mode change was handled successfully, false otherwise.
     */
    virtual bool handleChangeMode
    (
        const ChangeModeMsg *mode_msg
    ) = 0;

    /**
     * @brief Handles system command messages received by the agent through the agent control socket.
     *
     * @param[in] command_msg The system command message to be processed.
     *
     * @return true if the system command was handled successfully, false otherwise.
     */
    virtual bool handleSystemCommand
    (
        const SystemCommandMsg *command_msg
    ) = 0;

    // ROUTER-DEALER pattern helper methods
    inline void sendHeartbeat();

    inline bool registerAgent();

    std::atomic<bool> registered_ = false;
};
} // namespace PiTrac

#endif // GSAgent_H