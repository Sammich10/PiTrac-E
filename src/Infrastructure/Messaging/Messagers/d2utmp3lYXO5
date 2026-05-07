#ifndef ZMQ_MESSENGER_H
#define ZMQ_MESSENGER_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include "Infrastructure/Messaging/Messages/MessageFactory.h"
#include <zmq.h>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>

namespace PiTrac
{
class MessagerBase
{
  public:
    enum class SocketType
    {
        Publisher,
        Subscriber,
        Request,
        Reply,
        Push,
        Pull,
        Router,
        Dealer
    };

    MessagerBase
    (
        SocketType type
    );

    ~MessagerBase();

    static void createContext()
    {
        if (!context_)
        {
            context_ = zmq_ctx_new();
            if (!context_)
            {
                throw std::runtime_error("Failed to create ZMQ context");
            }
            context_ref_count_ = 0;
        }
        context_ref_count_++;
    }

    static void destroyContext()
    {
        if (context_ && context_ref_count_ > 0)
        {
            context_ref_count_--;
            if (context_ref_count_ == 0)
            {
                zmq_ctx_destroy(context_);
                context_ = nullptr;
            }
        }
    }

    void setTimeout
    (
        const int timeout_ms
    );

    int getTimeout() const;

    void bind
    (
        const std::string &endpoint
    );

    void connect
    (
        const std::string &endpoint
    );

    void disconnect
    (
        const std::string &endpoint
    );

    void subscribe
    (
        const std::string &topic = ""
    );

    virtual void sendMessage
    (
        const MessageInterface &message
    );

    virtual void sendMessage
    (
        const MessageInterface &message,
        const std::string &topic
    );

    // Structure to hold message with sender identity (used by router/dealer
    // classes)
    struct IdentityMessage
    {
        std::string sender_identity;
        std::unique_ptr<MessageInterface> message;
    };

    template<typename MessageType>
    std::unique_ptr<MessageType> receiveMessage(int timeout_ms = -1)
    {
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        int rc = zmq_msg_recv(&msg, socket_, 0);
        if (rc < 0)
        {
            zmq_msg_close(&msg);
            if (errno == EAGAIN)
            {
                return nullptr; // Timeout
            }
            throw std::runtime_error("Failed to receive message: " + std::string(zmq_strerror(
                                                                                     errno)));
        }

        auto message = std::make_unique<MessageType>();
        message->fromZmqMessage(msg);
        zmq_msg_close(&msg);

        return message;
    }

    virtual void startReceiving
    (
        std::function<void(std::unique_ptr<MessageInterface>)> handler
    );

    void stop();

  protected:
    // Protected members for derived classes
    void *getSocket()
    {
        return socket_;
    }

    SocketType getSocketType() const
    {
        return socket_type_;
    }

    bool isRunning() const
    {
        return running_.load();
    }

    void setRunning(bool running)
    {
        running_.store(running);
    }

    MessageFactory message_factory_ = MessageFactory();

    virtual void receiveLoop();

    // Hook for derived classes to perform actions after a message is
    // successfully received
    virtual void onMessageReceived()
    {
    }

    // Hook for derived classes to perform actions after a message is
    // successfully sent
    virtual void onMessageSent()
    {
    }

    // Thread management - accessible to derived classes
    std::thread receive_thread_;
    std::function<void(std::unique_ptr<MessageInterface>)> message_handler_;
    std::function<void(std::unique_ptr<IdentityMessage>)> identity_message_handler_;
    std::shared_ptr<GSLogger> logger_;

  private:
    static void *context_;
    static int context_ref_count_;
    void *socket_;
    SocketType socket_type_;
    std::atomic<bool> running_;
    int timeout_ms_ = 1000; // Default: 1 second
}; // class MessagerBase
} // namespace PiTrac

#endif // ZMQ_MESSENGER_H