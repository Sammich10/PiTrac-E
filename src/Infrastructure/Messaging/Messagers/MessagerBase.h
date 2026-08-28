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

    bool isInitialized() const
    {
        return initialized_;
    }

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

    void setTimeout(int timeout_ms)
    {
        poll_timeout_ = timeout_ms;
        zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &poll_timeout_, sizeof(poll_timeout_));
    }

    const int getTimeout() const
    {
        return poll_timeout_;
    }

    virtual RequestStatus sendMessage
    (
        const MessageInterface &message,
        const std::string &extra = ""
    );

    virtual RequestStatus pollMessage
    (
        std::unique_ptr<MessageInterface> &message
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

    virtual RequestStatus recvMessage
    (
        void *socket,
        std::unique_ptr<MessageInterface> &message
    );

    // Thread management - accessible to derived classes
    std::thread receive_thread_;
    std::function<void(std::unique_ptr<MessageInterface>)> message_handler_;
    std::shared_ptr<GSLogger> logger_;
    MessageFactory message_factory_;

    void *socket_;
    SocketType socket_type_;
    std::atomic<bool> running_;
    zmq_pollitem_t poll_item_[1];
    int poll_timeout_ = 1000;
    bool initialized_ = false;

  private:
    static void *context_;
    static int context_ref_count_;
}; // class MessagerBase
} // namespace PiTrac

#endif // ZMQ_MESSENGER_H