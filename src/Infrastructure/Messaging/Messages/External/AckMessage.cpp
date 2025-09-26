#include "Infrastructure/Messaging/Messages/External/AckMessage.h"

namespace PiTrac
{

void AckMessage::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_array(3);
    packCommonFields(packer);
    packer.pack(static_cast<int>(status_));
    // Serialize original message
    original_msg_->serialize(buffer);
}

void AckMessage::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 3)
    {
        throw std::runtime_error("Invalid AckMessage format");
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

    // Deserialize original message
    const char* original_data = obj.via.array.ptr[3].via.bin.ptr;
    size_t original_size = obj.via.array.ptr[3].via.bin.size;
    original_msg_->deserialize(original_data, original_size);
}

std::unique_ptr<MessageInterface> AckMessage::clone() const
{
    auto cloned = std::make_unique<AckMessage>(original_msg_->clone(), status_);
    cloned->timestamp_ = timestamp_;
    return cloned;
}

std::string AckMessage::toString() const
{
    return "AckMessage: Status=" + std::to_string(static_cast<int>(status_)) +
           ", OriginalMessageType=" + std::to_string(static_cast<int>(original_msg_->getMessageType()));

}

} // namespace PiTrac