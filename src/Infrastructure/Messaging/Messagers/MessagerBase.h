#ifndef ZMQ_MESSENGER_H
#define ZMQ_MESSENGER_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include "Infrastructure/Messaging/Messages/MessageFactory.h"
#include <zmq.h>
#include <memory>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>

namespace PiTrac
{
class MessagerBase
{
  public:
    enum class RequestStatus
    {
        Success,
        Timeout,
        Error
    };

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

    static RequestStatus createContext();

    static void destroyContext();

    void setTimeout
    (
        const int timeout_ms
    );

    int getTimeout() const;

    RequestStatus bind
    (
        const std::string &endpoint
    );

    RequestStatus connect
    (
        const std::string &endpoint
    );

    RequestStatus disconnect
    (
        const std::string &endpoint
    );

    virtual RequestStatus sendMessage
    (
        const MessageInterface &message,
        const std::string &extra = ""
    );

    virtual RequestStatus recvMessage
    (
        std::unique_ptr<MessageInterface> &message,
        int timeout_ms = 1000
    );

    virtual void startReceiving
    (
        std::function<void(std::unique_ptr<MessageInterface>)> handler
    );

    void stop();

  protected:
    // Protected members for derived classes
    inline void *getSocket()
    {
        return socket_;
    }

    inline SocketType getSocketType() const
    {
        return socket_type_;
    }

    inline bool isRunning() const
    {
        return running_.load();
    }

    inline void setRunning(bool running)
    {
        running_.store(running);
    }

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
    std::shared_ptr<GSLogger> logger_;
    MessageFactory message_factory_;

    void *socket_;
    SocketType socket_type_;
    std::atomic<bool> running_;
    int timeout_ms_ = 1000;

  private:
    static void *context_;
    static int context_ref_count_;
}; // class MessagerBase
} // namespace PiTrac

#endif // ZMQ_MESSENGER_H