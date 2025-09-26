#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"

namespace PiTrac
{
void ChangeModeMsg::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_array(3);
    packCommonFields(packer);
    packer.pack(static_cast<int>(newMode_));
}

void ChangeModeMsg::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();
    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 3)
    {
        throw std::runtime_error("Invalid ChangeModeMsg format");
    }

    int message_type;
    int64_t timestamp_ms;
    int mode_int;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(mode_int);

    if (static_cast<Message_Type>(message_type) != getMessageType())
    {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }

    newMode_ = static_cast<SystemMode_Type>(mode_int);
    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
}

std::unique_ptr<MessageInterface> ChangeModeMsg::clone() const
{
    auto cloned = std::make_unique<ChangeModeMsg>(newMode_);
    cloned->timestamp_ = timestamp_;
    cloned->newMode_ = newMode_;
    return cloned;
}

std::string ChangeModeMsg::toString() const
{
    return "ChangeMode: " + std::to_string(static_cast<int>(newMode_));
}
}