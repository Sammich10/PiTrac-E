#include "Infrastructure/Messaging/Messages/External/SystemCommandMsg.h"

namespace PiTrac
{
void SystemCommandMsg::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(&buffer);
    size_t message_size = 3;
    switch(command_id_)
    {
        case CommandID::SetMode:
            message_size += 1; // SetModePayload has 1 field
            break;
        case CommandID::Calibrate:
            // No payload for Calibrate command yet
            break;
        default:
            throw std::runtime_error("Unknown CommandID in SystemCommandMsg serialization");
    }
    packer.pack_array(message_size);
    packCommonFields(packer);
    packer.pack(static_cast<int>(command_id_));
    switch(command_id_)
    {
        case CommandID::SetMode:
        {
            const auto &payload = std::get<SetModePayload>(payload_);
            packer.pack(static_cast<int>(payload.mode));
            break;
        }
        case CommandID::Calibrate:
            // No payload for Calibrate command yet
            break;
        default:
            throw std::runtime_error("Unknown CommandID in SystemCommandMsg serialization");
    }
}

void SystemCommandMsg::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY)
    {
        throw std::runtime_error("Invalid message format");
    }

    msgpack::object_array arr = obj.via.array;
    if (arr.size < 3)
    {
        throw std::runtime_error("Invalid message size for SystemCommandMsg");
    }
    int message_type;
    int64_t timestamp_ms;
    int command_id_int;

    arr.ptr[0].convert(message_type);
    arr.ptr[1].convert(timestamp_ms);
    arr.ptr[2].convert(command_id_int);

    command_id_ = static_cast<CommandID>(command_id_int);

    switch(command_id_)
    {
        case CommandID::SetMode:
            if (arr.size != 4)
            {
                throw std::runtime_error("Invalid SetModePayload size in SystemCommandMsg");
            }
            payload_ = SetModePayload{};
            int mode_int;
            arr.ptr[3].convert(mode_int);
            std::get<SetModePayload>(payload_).mode = static_cast<SystemMode_Type>(mode_int);
            break;
        case CommandID::Calibrate:
            if (arr.size != 3)
            {
                throw std::runtime_error("Invalid Calibrate command size in SystemCommandMsg");
            }
            // No payload for Calibrate command yet
            break;
        default:
            throw std::runtime_error("Unknown CommandID in SystemCommandMsg deserialization: " + std::to_string(command_id_int));
    }
}

std::unique_ptr<MessageInterface> SystemCommandMsg::clone() const
{
    auto cloned = std::make_unique<SystemCommandMsg>(command_id_, payload_);
    cloned->timestamp_ = timestamp_;
    return cloned;
}

std::string SystemCommandMsg::toString() const
{
    std::string result = "SystemCommandMsg: CommandID=" + std::to_string(static_cast<int>(command_id_)) + ", Payload=";
    switch(command_id_)
    {
        case CommandID::SetMode:
        {
            const auto &payload = std::get<SetModePayload>(payload_);
            result += "{ mode: " + std::to_string(static_cast<int>(payload.mode)) + " }";
            break;
        }
        case CommandID::Calibrate:
            result += "{ Calibrate command has no payload yet }";
            break;
        default:
            result += "Unknown CommandID";
            break;
    }
    return result;
}
} // namespace PiTrac