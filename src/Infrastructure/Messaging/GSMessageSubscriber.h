#ifndef GS_MESSAGE_SUBSCRIBER_H
#define GS_MESSAGE_SUBSCRIBER_H

#include <zmq.hpp>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include "Common/Utils/Logging/GSLogger.h"
#include "Infrastructure/Messaging/Messages/GSMessageBase.h"

namespace PiTrac
{

// Callback function type for message handling
using MessageCallback = std::function<void(const std::string& topic, const std::string& message)>;

class GSMessageSubscriber
{
public:
    /**
     * @brief Constructs a ZeroMQ message subscriber
     * @param subscribeEndpoint Endpoint for subscribing to messages (e.g., "tcp://localhost:5555")
     */
    explicit GSMessageSubscriber(const std::string& subscribeEndpoint, const std::string &topic);
    
    virtual ~GSMessageSubscriber();

    /**
     * @brief Subscribe to messages on a specific topic
     * @param topic Topic to subscribe to (empty string subscribes to all)
     * @param callback Function to call when message is received
     */
    void subscribe(const std::string& topic, MessageCallback callback);
    
    /**
     * @brief Unsubscribe from a specific topic
     * @param topic Topic to unsubscribe from
     */
    void unsubscribe(const std::string& topic);
    
    /**
     * @brief Start the subscriber thread
     * @return True if subscriber started successfully
     */
    bool start();
    
    /**
     * @brief Stop the subscriber thread
     */
    void stop();
    
    /**
     * @brief Check if subscriber is running
     */
    bool isRunning() const { return subscriberRunning_; }

protected:
    /**
     * @brief Called when a message is received (override in derived classes)
     * @param topic Message topic
     * @param message Message content
     */
    virtual void onMessageReceived(const std::string& topic, const std::string& message);

private:
    // ZeroMQ context and socket
    std::unique_ptr<zmq::context_t> context_;
    std::unique_ptr<zmq::socket_t> subscribeSocket_;
    
    // Configuration
    std::string subscribeEndpoint_;
    GSLogger logger_;

    // State tracking
    std::atomic<bool> subscriberRunning_;
    std::atomic<bool> shouldStop_;
    
    // Subscription management
    std::mutex callbackMutex_;
    std::unordered_map<std::string, std::vector<MessageCallback>> topicCallbacks_;
    
    // Subscriber thread
    std::unique_ptr<std::thread> subscriberThread_;
    
    // Helper methods
    bool initialize();
    void subscriberLoop();
    void processReceivedMessage(const std::string& topic, const std::string& message);
};

// Mixin class for easy inheritance
class GSMessageSubscriberMixin
{
public:
    explicit GSMessageSubscriberMixin(const std::string& subscribeEndpoint)
        : messageSubscriber_(std::make_unique<GSMessageSubscriber>(subscribeEndpoint)) {}
    
    virtual ~GSMessageSubscriberMixin() = default;

protected:
    void subscribe(const std::string& topic, MessageCallback callback) {
        messageSubscriber_->subscribe(topic, callback);
    }
    
    void unsubscribe(const std::string& topic) {
        messageSubscriber_->unsubscribe(topic);
    }
    
    bool startSubscriber() {
        return messageSubscriber_->start();
    }
    
    void stopSubscriber() {
        messageSubscriber_->stop();
    }
    
    bool isSubscriberRunning() const {
        return messageSubscriber_->isRunning();
    }

private:
    std::unique_ptr<GSMessageSubscriber> messageSubscriber_;
};

} // namespace PiTrac

#endif // GS_MESSAGE_SUBSCRIBER_H