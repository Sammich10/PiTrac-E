#ifndef GS_MESSAGE_PUBLISHER_H
#define GS_MESSAGE_PUBLISHER_H

#include "Infrastructure/Messaging/Messages/GSMessageBase.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <zmq.hpp>
#include <string>
#include <memory>
#include <atomic>

namespace PiTrac
{

class GSMessagePublisher
{
public:
    /**
     * @brief Constructs a ZeroMQ message publisher
     * @param publishEndpoint Endpoint for publishing messages (e.g., "tcp://*:5555")
     */
    explicit GSMessagePublisher(const std::string& publishEndpoint);

    virtual ~GSMessagePublisher();

    /**
     * @brief Publish a message to a specific topic
     * @param topic Message topic/channel
     * @param message Message object to publish
     * @return True if message was sent successfully
     */
    bool publishMessage(const std::string& topic, const GSMessageBase& message);
    
    /**
     * @brief Publish a raw string message to a specific topic
     * @param topic Message topic/channel
     * @param message Raw message string
     * @return True if message was sent successfully
     */
    bool publishRawMessage(const std::string& topic, const std::string& message);
    
    /**
     * @brief Check if publisher is ready
     */
    bool isReady() const { return publisherReady_; }

private:
    // ZeroMQ context and socket
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> publishSocket_;
    
    // Configuration
    std::string publishEndpoint_;
    
    // State tracking
    std::atomic<bool> publisherReady_;

    // Helper methods
    bool initialize();
};

// Mixin class for easy inheritance
class GSMessagePublisherMixin
{
public:
    explicit GSMessagePublisherMixin(const std::string& publishEndpoint = "tcp://*:5555")
        : messagePublisher_(std::make_unique<GSMessagePublisher>(publishEndpoint)) {}
    
    virtual ~GSMessagePublisherMixin() = default;

protected:
    bool publishMessage(const std::string& topic, const GSMessageBase& message) {
        return messagePublisher_->publishMessage(topic, message);
    }
    
    bool publishRawMessage(const std::string& topic, const std::string& message) {
        return messagePublisher_->publishRawMessage(topic, message);
    }
    
    bool isPublisherReady() const {
        return messagePublisher_->isReady();
    }

private:
    std::unique_ptr<GSMessagePublisher> messagePublisher_;
};

} // namespace PiTrac

#endif // GS_MESSAGE_PUBLISHER_H