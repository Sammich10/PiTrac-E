#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

#include "IMessage.h"
#include "Infrastructure/Messages/Messages/*.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace PiTrac
{
class MessageFactory
{
  private:
    std::unordered_map<std::string, std::function<std::unique_ptr<IMessage>()> > creators_;

  public:
    MessageFactory()
    {
        // Register message types
    }

    template<typename MessageType>
    void registerMessage(const std::string &type)
    {
        creators_[type] = []() {
                              return std::make_unique<MessageType>();
                          };
    }

    std::unique_ptr<IMessage> createFromZmqMessage(const zmq_msg_t &msg)
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

        std::string message_type;
        obj.via.array.ptr[0].convert(message_type);

        auto it = creators_.find(message_type);
        if (it == creators_.end())
        {
            throw std::runtime_error("Unknown message type: " + message_type);
        }

        auto message = it->second();
        message->fromZmqMessage(msg);
        return message;
    }
};
} // namespace PiTrac

#endif // MESSAGE_FACTORY_H