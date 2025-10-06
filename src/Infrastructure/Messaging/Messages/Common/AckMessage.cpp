#include "Infrastructure/Messaging/Messages/Common/AckMessage.h"

namespace PiTrac
{

void AckMessage::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_array(3); // Just message type, timestamp, and status
    packCommonFields(packer);
    packer.pack(static_cast<int>(status_));
    // Note: We don't serialize the original message to avoid circular dependencies
    // The original message is kept for local reference but not transmitted
}

void AckMessage::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 3)
    {
        throw std::runtime_error("Invalid AckMessage format - expected array with 3 elements, got " + 
                                std::to_string(obj.via.array.size));
    }

    int message_type;
    int64_t timestamp_ms;
    int status_int_;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(status_int_);
    status_ = static_cast<Status>(status_int_);

    if (static_cast<Message_Type>(message_type) != getMessageType())
    {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }

    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));

    // Note: Original message is not serialized/deserialized to avoid complexity
    // It's kept as a local reference during creation but not transmitted
    original_msg_ = nullptr;
}

std::unique_ptr<MessageInterface> AckMessage::clone() const
{
    auto cloned = std::make_unique<AckMessage>();
    cloned->status_ = status_;
    cloned->timestamp_ = timestamp_;
    cloned->original_msg_ = original_msg_ ? original_msg_->clone() : nullptr;
    return cloned;
}

std::string AckMessage::toString() const
{
    return "AckMessage: Status=" + std::to_string(static_cast<int>(status_)) +
           ", OriginalMessageType=" + (original_msg_ ? 
               std::to_string(static_cast<int>(original_msg_->getMessageType())) : "None");
}

} // namespace PiTrac