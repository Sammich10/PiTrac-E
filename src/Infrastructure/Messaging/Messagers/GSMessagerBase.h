#ifndef ZMQ_MESSENGER_H
#define ZMQ_MESSENGER_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include "Infrastructure/Messaging/Messages/MessageFactory.h"
#include <zmq.h>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace PiTrac
{
class GSMessagerBase
{
  private:
    static void *context_;
    void *socket_;
    std::atomic<bool> running_;
    std::thread receive_thread_;
    std::function<void(std::unique_ptr<MessageInterface>)> message_handler_;

  public:
    enum class SocketType
    {
        Publisher,
        Subscriber,
        Request,
        Reply,
        Push,
        Pull
    };

    GSMessagerBase
    (
        SocketType type
    );

    ~GSMessagerBase();

    static void createContext()
    {
        if (!context_)
        {
            context_ = zmq_ctx_new();
            if (!context_)
            {
                throw std::runtime_error("Failed to create ZMQ context");
            }
        }
    }

    static void destroyContext()
    {
        if (context_)
        {
            zmq_ctx_destroy(context_);
            context_ = nullptr;
        }
    }

    void bind
    (
        const std::string &endpoint
    );

    void connect
    (
        const std::string &endpoint
    );

    void subscribe
    (
        const std::string &topic = ""
    );

    void sendMessage
    (
        const MessageInterface &message
    );

    void sendMessage
    (
        const MessageInterface &message,
        const std::string &topic
    );

    template<typename MessageType>
    std::unique_ptr<MessageType> receiveMessage(int timeout_ms = -1)
    {
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        // Set receive timeout if specified
        if (timeout_ms >= 0)
        {
            zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        }

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

    void startReceiving
    (
        std::function<void(std::unique_ptr<MessageInterface>)> handler
    );

    void stop();

  private:

    MessageFactory message_factory_ = MessageFactory();

    void receiveLoop();
}; // class GSMessagerBase
} // namespace PiTrac

#endif // ZMQ_MESSENGER_H