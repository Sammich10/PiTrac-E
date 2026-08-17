#ifndef GS_MESSAGE_INTERFACFE_H
#define GS_MESSAGE_INTERFACFE_H

#include <string>
#include <chrono>
#include <memory>
#include <zmq.h>
#include <msgpack.hpp>
#include "Infrastructure/Messaging/Messages/MessageTypes.h"

namespace PiTrac
{
class MessageInterface
{
  public:
    virtual ~MessageInterface() = default;

    // Core message operations
    virtual Message_Type getMessageType() const = 0;
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;
    virtual void setTimestamp
    (
        const std::chrono::system_clock::time_point &timestamp
    ) = 0;

    // Serialization interface
    virtual void serialize
    (
        msgpack::sbuffer &buffer
    ) const = 0;

    virtual void deserialize
    (
        const char *data,
        size_t size
    ) = 0;

    // ZMQ message operations
    virtual void toZmqMessage
    (
        zmq_msg_t &msg
    ) const = 0;

    virtual void fromZmqMessage
    (
        zmq_msg_t &msg
    ) = 0;

    // Convenience methods
    virtual std::string toString() const = 0;
    virtual std::unique_ptr<MessageInterface> clone() const = 0;

    virtual bool isValid() const
    {
        return is_valid_;
    }

    bool hasIdentity() const
    {
        return has_identity_;
    }

    const std::string &getIdentity() const
    {
        return identity_;
    }

    void setIdentity(const std::string &identity)
    {
        identity_ = identity;
        has_identity_ = true;
    }

  protected:
    const std::string incorrectMessageTypeString(const Message_Type incorrectMessageType) const
    {
        return "Message type mismatch: expected " + std::to_string(static_cast<int>(getMessageType())) +
               ", got " + std::to_string(static_cast<int>(incorrectMessageType));
    }

    // Flag to indicate if the message is valid (e.g., after deserialization) TODO: Implement validation logic in derived classes
    bool is_valid_ = true;
    std::string error_message_ = "";
    bool has_identity_ = false;
    std::string identity_ = "";
};
} // namespace PiTrac

#endif // GS_MESSAGE_INTERFACFE_H