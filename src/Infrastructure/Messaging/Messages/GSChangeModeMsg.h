#ifndef GS_HOST_COMMAND_MSG_H
#define GS_HOST_COMMAND_MSG_H

#include "Infrastructure/Messaging/Messages/GSMessageBase.h"
#include "Common/System/SystemModes.h"

namespace PiTrac
{

class GSChangeModeMsg : public GSMessageBase
{
public:

GSChangeModeMsg() = default;

GSChangeModeMsg(SystemMode_Type newMode) 
: newMode_(newMode) 
{}

GSMessageType getMessageType() const override
{
    return GSMessageType::ChangeMode;
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

std::unique_ptr<GSMessageInterface> clone() const override;

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