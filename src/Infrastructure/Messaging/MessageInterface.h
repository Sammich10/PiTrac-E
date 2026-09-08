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
  
    /**
     * @brief Virtual destructor for the message interface. Ensures proper cleanup of derived classes.
     */
    virtual ~MessageInterface() = default;

    /**
     * @brief Get the type of the message.
     *
     * @return The message type as an enum value of Message_Type.
     */
    virtual Message_Type getMessageType() const = 0;
    
    /**
     * @brief Get the timestamp of the message.
     *
     * @return The timestamp as a std::chrono::system_clock::time_point.
     */
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;

    /**
     * @brief Convert the message to a ZMQ message.
     *
     * @param[out] msg The ZMQ message to populate with the serialized data.
     * 
     * @return true if the message was successfully converted to a ZMQ message, false otherwise.
     * @throws none
     * @note The method should not throw exceptions; any serialization errors should result in a false return value.
     */
    virtual bool toZmqMessage
    (
        zmq_msg_t &msg
    ) const = 0;

    /**
     * @brief Populate the message class from a ZMQ message.
     *
     * @param[in] msg The ZMQ message containing the serialized data.
     * 
     * @return true if the message was successfully populated from the ZMQ message, false otherwise.
     * @throws none
     * @note The method should not throw exceptions; any deserialization errors should result in a false return value.
     */
    virtual bool fromZmqMessage
    (
        zmq_msg_t &msg
    ) = 0;

    /**
     * @brief Get a string representation of the message.
     *
     * @return A string describing the message, typically including its type and timestamp.
     */
    virtual std::string toString() const = 0;

    /**
     * @brief Create a deep copy of the message.
     *
     * @return A unique pointer to the cloned message instance.
     */
    virtual std::unique_ptr<MessageInterface> clone() const = 0;

    /**
     * @brief Check if the message is valid.
     *
     * @return true if the message is valid, false otherwise.
     */
    virtual bool isValid() const
    {
        return is_valid_;
    }

    /**
     * @brief Check if the message has an identity set.
     * 
     * @return true if the message has an identity, false otherwise.
     * @note The identity must be set using setIdentity() before this method returns true. This method is used for ROUTER/DEALER socket communication patterns.
     */
    bool hasIdentity() const
    {
        return has_identity_;
    }

    /**
     * @brief Get the identity of the message.
     *
     * @return The identity string of the message.
     */
    const std::string &getIdentity() const
    {
        return identity_;
    }

    /**
     * @brief Set the identity of the message.
     *
     * @param[in] identity The identity string to set for the message.
     */
    void setIdentity(const std::string &identity)
    {
        identity_ = identity;
        has_identity_ = true;
    }

  protected:

    /**
     * @brief Default constructor for the message interface. Allows derived classes to be instantiated.
     */
    MessageInterface() = default;
  
    /**
     * @brief Serialize the message into a MsgPack buffer.
     *
     * @param[in,out] buffer The MsgPack buffer to serialize the message into.
     * @note This method must be implemented by derived classes to provide message-specific serialization logic.
     * 
     * @throws runtime_error if serialization fails.
     */
    virtual void serialize
    (
        msgpack::sbuffer &buffer
    ) const = 0;

    /**
     * @brief Deserialize the message from a MsgPack buffer.
     *
     * @param[in] data The pointer to the serialized message data.
     * @param[in] size The size of the serialized message data.
     * @note This method must be implemented by derived classes to provide message-specific deserialization logic.
     *
     * @throws runtime_error if deserialization fails.
     */
    virtual void deserialize
    (
        const char *data,
        size_t size
    ) = 0;

    /**
     * @brief Generate an error message string for an incorrect message type.
     *
     * @param[in] incorrectMessageType The message type that was incorrect.
     * @return A string describing the message type mismatch.
     */
    const std::string incorrectMessageTypeString(const Message_Type incorrectMessageType) const
    {
        return "Message type mismatch: expected " + std::to_string(static_cast<int>(getMessageType())) +
               ", got " + std::to_string(static_cast<int>(incorrectMessageType));
    }

    /**
     * @brief Flag to indicate if the message is valid (e.g., after deserialization)
     */
    bool is_valid_ = true;
    
    /**
     * @brief Error message associated with the message validity
     */
    std::string error_message_ = "";
    
    /**
     * @brief Flag to indicate if the message has an identity
     */
    bool has_identity_ = false;
    
    /**
     * @brief Identity string of the message
     */
    std::string identity_ = "";
};
} // namespace PiTrac

#endif // GS_MESSAGE_INTERFACFE_H