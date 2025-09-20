#ifndef GS_REGISTER_TASK_MSG_H
#define GS_REGISTER_TASK_MSG_H

#include "Infrastructure/Messaging/Messages/MessageBase.h"

namespace PiTrac
{

class RegisterTaskMsg : public MessageBase
{
public:

RegisterTaskMsg() = default;

RegisterTaskMsg(const int64_t &taskPid, const std::string &taskName)
: taskPid_(taskPid)
, taskName_(taskName)
{}

Message_Type getMessageType() const override
{
    return Message_Type::RegisterTask;
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

uint64_t getTaskPid() const
{
    return taskPid_;
}

void setTaskPid(const uint64_t &pid)
{
    taskPid_ = pid;
}

std::string getTaskName() const
{
    return taskName_;
}

void setTaskName(const std::string &taskName)
{
    taskName_ = taskName;
}

std::string toString() const override;

private:
    uint64_t taskPid_;
    std::string taskName_;

};

} // namespace PiTrac

#endif // GS_REGISTER_TASK_MSG_H
