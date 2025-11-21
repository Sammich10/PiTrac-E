#include "Infrastructure/Messaging/Messages/Internal/HeartbeatMsg.h"
#include "Common/System/System.h"

namespace PiTrac
{
void HeartbeatMsg::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);
    packer.pack_array(6);
    packCommonFields(packer);
    packer.pack(pid_);
    packer.pack(agent_name_);
    packer.pack(static_cast<int>(status_));
    packer.pack(static_cast<int>(mode_));
}

void HeartbeatMsg::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();
    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 6)
    {
        throw std::runtime_error("Invalid HeartbeatMsg format");
    }

    int message_type;
    int64_t timestamp_ms;
    int status_int;
    int mode_int;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(pid_);
    obj.via.array.ptr[3].convert(agent_name_);
    obj.via.array.ptr[4].convert(status_int);
    obj.via.array.ptr[5].convert(mode_int);

    if (static_cast<Message_Type>(message_type) != getMessageType())
    {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }

    status_ = static_cast<TaskStatus>(status_int);
    mode_ = static_cast<SystemMode_Type>(mode_int);
    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
}

std::unique_ptr<MessageInterface> HeartbeatMsg::clone() const
{
    auto cloned = std::make_unique<HeartbeatMsg>(pid_, agent_name_, status_, mode_);
    cloned->timestamp_ = timestamp_;
    return std::move(cloned);
}

std::string HeartbeatMsg::toString() const
{
    auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
    return "HeartbeatMsg[PID:" + std::to_string(pid_) +
           ", Agent:" + agent_name_ +
           ", Status:" + std::to_string(static_cast<int>(status_)) +
           ", Mode:" + System::systemModeToString(mode_) +
           ", Time:" + std::to_string(time_t) + "]";
}
}