#include "Infrastructure/Messaging/Messages/GSMessageFactory.h"
#include "Infrastructure/Messaging/Messages/GSMessageBase.h"
#include "Infrastructure/Messaging/Messages/GSCameraFrameMsg.h"
#include "Infrastructure/Messaging/Messages/GSCameraFrameRawMsg.h"
#include "Infrastructure/Messaging/Messages/GSChangeModeMsg.h"

namespace PiTrac
{
GSMessageFactory::GSMessageFactory()
{
    // Register message types
    registerMessage<GSCameraFrameMessage>(GSMessageType::CameraFrame);
    // registerMessage<GSCameraFrameRawMessage>(GSMessageType::CameraFrameRaw);
    registerMessage<GSChangeModeMsg>(GSMessageType::ChangeMode);
}

std::unique_ptr<GSMessageInterface> GSMessageFactory::createFromZmqMessage(zmq_msg_t &msg)
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

    auto it = creators_.find(static_cast<GSMessageType>(message_type));
    if (it == creators_.end())
    {
        throw std::runtime_error("Unknown message type: " + std::to_string(static_cast<int>(message_type)));
    }

    auto message = it->second();
    message->fromZmqMessage(msg);
    return message;
}
} // namespace PiTrac