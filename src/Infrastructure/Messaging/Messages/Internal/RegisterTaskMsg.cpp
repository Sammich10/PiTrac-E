#include "Infrastructure/Messaging/Messages/Internal/RegisterTaskMsg.h"

namespace PiTrac
{
void RegisterTaskMsg::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_array(4);
    packCommonFields(packer);
    packer.pack(taskPid_);
    packer.pack(taskName_);
}

void RegisterTaskMsg::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();
    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 4)
    {
        throw std::runtime_error("Invalid RegisterTaskMsg format");
    }

    int message_type;
    int64_t timestamp_ms;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(taskPid_);
    obj.via.array.ptr[3].convert(taskName_);

    if (static_cast<Message_Type>(message_type) != getMessageType())
    {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }

    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
}

std::unique_ptr<MessageInterface> RegisterTaskMsg::clone() const
{
    auto cloned = std::make_unique<RegisterTaskMsg>(taskPid_, taskName_);
    cloned->timestamp_ = timestamp_;
    cloned->taskPid_ = taskPid_;
    cloned->taskName_ = taskName_;
    return cloned;
}

std::string RegisterTaskMsg::toString() const
{
    return "RegisterTask: ID=" + std::to_string(taskPid_) + ", Name=" + taskName_;
}
} // namespace PiTrac