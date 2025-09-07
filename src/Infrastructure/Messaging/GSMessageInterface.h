#ifndef GS_MESSAGE_INTERFACFE_H
#define GS_MESSAGE_INTERFACFE_H

#include <string>
#include <chrono>
#include <memory>
#include <zmq.h>
#include <msgpack.hpp>

namespace PiTrac
{
class GSMessageInterface
{
  public:
    virtual ~GSMessageInterface() = default;

    // Core message operations
    virtual std::string getMessageType() const = 0;
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;
    virtual void setTimestamp(const std::chrono::system_clock::time_point &timestamp) = 0;

    // Serialization interface
    virtual void serialize(msgpack::sbuffer &buffer) const = 0;
    virtual void deserialize(const char *data, size_t size) = 0;

    // ZMQ message operations
    virtual void toZmqMessage(zmq_msg_t &msg) const = 0;
    virtual void fromZmqMessage(zmq_msg_t &msg) = 0;

    // Convenience methods
    virtual std::string toString() const = 0;
    virtual std::unique_ptr<GSMessageInterface> clone() const = 0;
};
} // namespace PiTrac

#endif // GS_MESSAGE_INTERFACFE_H