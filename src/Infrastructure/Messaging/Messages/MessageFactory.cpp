#include "Infrastructure/Messaging/Messages/MessageBase.h"
#include "Infrastructure/Messaging/Messages/MessageFactory.h"
#include "Infrastructure/Messaging/Messages/External/CameraFrameMsg.h"
#include "Infrastructure/Messaging/Messages/External/SystemCommandMsg.h"
#include "Infrastructure/Messaging/Messages/External/AckMessage.h"
#include "Infrastructure/Messaging/Messages/Internal/ChangeModeMsg.h"
#include "Infrastructure/Messaging/Messages/Internal/RegisterTaskMsg.h"

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

    // Unpack just the first element to get message type
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size == 0)
    {
        throw std::runtime_error("Invalid message format");
    }

    int message_type;
    obj.via.array.ptr[0].convert(message_type);

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
    }
    return message;
}
} // namespace PiTrac