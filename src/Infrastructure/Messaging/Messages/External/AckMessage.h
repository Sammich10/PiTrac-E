#ifndef ACK_MESSAGE_H
#define ACK_MESSAGE_H

#include "Infrastructure/Messaging/Messages/MessageBase.h"

namespace PiTrac
{

class AckMessage : public MessageBase
{
public:

enum class Status
{
    Success = 0,
    Failure = 1,
    Invalid = 2,
    Timeout = 3
};

AckMessage() = default;

AckMessage(std::unique_ptr<MessageInterface> original_msg, Status status_code)
: status_(status_code)
, original_msg_(std::move(original_msg))
{
}

void serialize(msgpack::sbuffer &buffer) const override;

void deserialize(const char *data, size_t size) override;

Message_Type getMessageType() const override
{
    return Message_Type::AckMessage;
}

Status getStatus() const { return status_; }

Message_Type getAckedMessageType() const { return original_msg_->getMessageType(); }

std::unique_ptr<MessageInterface> clone() const override;

std::string toString() const override;

private:

Status status_;
std::unique_ptr<MessageInterface> original_msg_;

}; 

} // namespace PiTrac

#endif // ACK_MESSAGE_H