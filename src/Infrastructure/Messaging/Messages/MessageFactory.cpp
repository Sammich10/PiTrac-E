#include "Infrastructure/Messaging/Messages/MessageBase.h"
#include "Infrastructure/Messaging/Messages/MessageFactory.h"
#include "Infrastructure/Messaging/Messages/External/CameraFrameMsg.h"
#include "Infrastructure/Messaging/Messages/External/SystemCommandMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/RegisterTaskMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/HeartbeatMsg.h"
#include "Infrastructure/Messaging/Messages/Common/AckMessage.h"

namespace PiTrac
{
MessageFactory::MessageFactory()
{
    // Register message types
    registerMessage<CameraFrameMsg>(Message_Type::CameraFrame);
    // registerMessage<GSCameraFrameRawMessage>(Message_Type::CameraFrameRaw);
    registerMessage<ChangeModeMsg>(Message_Type::ChangeMode);
    registerMessage<RegisterTaskMsg>(Message_Type::RegisterTask);
    registerMessage<SystemCommandMsg>(Message_Type::SystemCommand);
    registerMessage<AckMessage>(Message_Type::AckMessage);
    registerMessage<HeartbeatMsg>(Message_Type::Heartbeat);
    // Future messages here...
}

std::unique_ptr<MessageInterface> MessageFactory::createFromZmqMessage(zmq_msg_t &msg)
{
    // Extract message type from the beginning of the message
    const char *data = static_cast<const char *>(zmq_msg_data(&msg));
    size_t size = zmq_msg_size(&msg);

    if (size == 0)
    {
        throw std::runtime_error("Empty message received");
    }

    if (data == nullptr)
    {
        throw std::runtime_error("Message data is null");
    }

    // Unpack just the first element to get message type
    msgpack::object_handle oh;
    try {
        oh = msgpack::unpack(data, size);
    } catch (const std::exception &e) {
        throw std::runtime_error("Failed to unpack MessagePack data: " + std::string(e.what()));
    } catch (...) {
        throw std::runtime_error("Failed to unpack MessagePack data: unknown exception");
    }
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size == 0)
    {
        throw std::runtime_error("Invalid message format");
    }

    int message_type;
    try {
        obj.via.array.ptr[0].convert(message_type);
    } catch (const std::exception &e) {
        throw std::runtime_error("Failed to extract message type: " + std::string(e.what()));
    }

    auto it = creators_.find(static_cast<Message_Type>(message_type));
    if (it == creators_.end())
    {
        throw std::runtime_error("Unknown message type: " + std::to_string(static_cast<int>(message_type)));
    }

    auto message = it->second();
    try {
        message->fromZmqMessage(msg);
    } catch (const std::exception &e) {
        logger_->error("Failed to deserialize message for type %d: %s", static_cast<int>(message_type), e.what());
        throw std::runtime_error("Failed to deserialize message: " + std::string(e.what()));
    }
    return message;
}
} // namespace PiTrac