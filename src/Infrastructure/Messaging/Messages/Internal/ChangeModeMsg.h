#ifndef GS_MODE_COMMAND_MSG_H
#define GS_MODE_COMMAND_MSG_H

#include "Infrastructure/Messaging/Messages/MessageBase.h"
#include "Common/System/System.h"

namespace PiTrac
{

class ChangeModeMsg : public MessageBase
{
public:

ChangeModeMsg() = default;

ChangeModeMsg(SystemMode_Type newMode) 
: newMode_(newMode) 
{}

Message_Type getMessageType() const override
{
    return Message_Type::ChangeMode;
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

SystemMode_Type getNewMode() const
{
    return newMode_;
}

void setNewMode(const SystemMode_Type mode)
{
    newMode_ = mode;
}

std::string toString() const override;

private:
    SystemMode_Type newMode_;

};

} // namespace PiTrac

#endif // GS_HOST_COMMAND_MSG_H