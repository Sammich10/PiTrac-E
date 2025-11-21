#ifndef HEARTBEAT_MSG_H
#define HEARTBEAT_MSG_H

#include "Infrastructure/Messaging/Messages/MessageBase.h"
#include "Infrastructure/TaskProcess/TaskStatus.h"
#include "Common/System/System.h"
#include <chrono>

namespace PiTrac
{
class HeartbeatMsg : public MessageBase
{
  public:
    HeartbeatMsg() = default;

    HeartbeatMsg(pid_t pid, const std::string &agent_name, TaskStatus status, SystemMode_Type mode)
        : pid_(pid)
        , agent_name_(agent_name)
        , status_(status)
        , mode_(mode)
        , timestamp_(std::chrono::system_clock::now())
    {
    }

    Message_Type getMessageType() const override
    {
        return Message_Type::Heartbeat;
    }

    void serialize
    (
        msgpack::sbuffer &buffer
    ) const override;

    void deserialize
    (
        const char *data,
        size_t size
    ) override;

    std::unique_ptr<MessageInterface> clone() const override;
    std::string toString() const override;

    // Getters
    pid_t getPid() const
    {
        return pid_;
    }

    const std::string &getAgentName() const
    {
        return agent_name_;
    }

    TaskStatus getStatus() const
    {
        return status_;
    }

    SystemMode_Type getMode() const
    {
        return mode_;
    }

    std::chrono::system_clock::time_point getTimestamp() const
    {
        return timestamp_;
    }

  private:
    pid_t pid_;
    std::string agent_name_;
    TaskStatus status_;
    SystemMode_Type mode_;
    std::chrono::system_clock::time_point timestamp_;
};
} // namespace PiTrac

#endif // HEARTBEAT_MSG_H