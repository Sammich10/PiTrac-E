#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include "Infrastructure/Messaging/GSMessageInterface.h"
#include <stdexcept>
#include <sstream>

namespace PiTrac
{
class GSMessageBase : public GSMessageInterface
{
  public:
    GSMessageBase();

    virtual ~GSMessageBase() = default;

    // Timestamp operations
    std::chrono::system_clock::time_point getTimestamp() const override;

    void setTimestamp
    (
        const std::chrono::system_clock::time_point &timestamp
    ) override;

    // ZMQ message operations implementation
    void toZmqMessage
    (
        zmq_msg_t &msg
    ) const override;

    void fromZmqMessage
    (
        zmq_msg_t &msg
    ) override;

    // Utility methods
    std::string toString() const override;

  protected:
    // Helper for serializing common fields
    template<typename Packer>
    void packCommonFields(Packer &packer) const
    {
        auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp_.time_since_epoch()).count();
        packer.pack(getMessageType());
        packer.pack(timestamp_ms);
    }

    // Helper for deserializing common fields
    template<typename Unpacker>
    void unpackCommonFields(Unpacker &unpacker)
    {
        std::string message_type;
        int64_t timestamp_ms;
        unpacker.unpack(message_type);
        unpacker.unpack(timestamp_ms);

        if (message_type != getMessageType())
        {
            throw std::runtime_error("Message type mismatch: expected " +
                                     getMessageType() + ", got " + message_type);
        }

        timestamp_ = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms));
    }

    std::chrono::system_clock::time_point timestamp_;
};
} // namespace PiTrac

#endif // MESSAGE_BASE_H