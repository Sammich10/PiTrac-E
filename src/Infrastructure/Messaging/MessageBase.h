#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include <stdexcept>
#include <sstream>

namespace PiTrac
{
class MessageBase : public MessageInterface
{
  public:
    MessageBase() = default;

    virtual ~MessageBase() = default;

    // Timestamp operations
    std::chrono::system_clock::time_point getTimestamp() const override
    {
        return timestamp_;
    }

    void setTimestamp
    (
        const std::chrono::system_clock::time_point &timestamp
    ) override
    {
        timestamp_ = timestamp;
    }

    // ZMQ message operations implementation
    void toZmqMessage
    (
        zmq_msg_t &msg
    ) const override
    {
        msgpack::sbuffer buffer;
        serialize(buffer);
        zmq_msg_init_size(&msg, buffer.size());
        memcpy(zmq_msg_data(&msg), buffer.data(), buffer.size());
    }

    void fromZmqMessage
    (
        zmq_msg_t &msg
    ) override
    {
        deserialize(static_cast<const char *>(zmq_msg_data(&msg)), zmq_msg_size(&msg));
    }

    // Utility methods
    std::string toString() const override
    {
        std::ostringstream oss;
        oss << "Message Type: " << static_cast<int>(getMessageType())
            << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
                   timestamp_.time_since_epoch()).count() << " ms since epoch";
        return oss.str();
    }
    std::unique_ptr<MessageInterface> clone() const override = 0;
  protected:
    // Helper for serializing common fields
    template<typename Packer>
    void packCommonFields(Packer &packer) const
    {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp_.time_since_epoch()).count();
        packer.pack(static_cast<int>(getMessageType()));
        packer.pack(timestamp_ms);
    }

    // Helper for deserializing common fields
    template<typename Unpacker>
    void unpackCommonFields(Unpacker &unpacker)
    {
        Message_Type message_type;
        int64_t timestamp_ms;
        unpacker.unpack(message_type);
        unpacker.unpack(timestamp_ms);

        if (message_type != getMessageType())
        {
            throw std::runtime_error("Message type mismatch: expected " +
                                     std::to_string(static_cast<int>(getMessageType())) + ", got " + std::to_string(static_cast<int>(message_type)));
        }

        timestamp_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms));
    }

    std::chrono::system_clock::time_point timestamp_;
};
} // namespace PiTrac

#endif // MESSAGE_BASE_H