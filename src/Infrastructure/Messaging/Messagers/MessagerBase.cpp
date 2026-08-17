#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{
void *MessagerBase::context_ = nullptr;
int MessagerBase::context_ref_count_ = 0;

MessagerBase::MessagerBase(SocketType type)
    : socket_(nullptr)
    , socket_type_(type)
    , running_(false)
    , timeout_ms_(1000)  // Default 1000ms timeout
    , logger_(GSLogger::getInstance())
{
    if (!context_)
    {
        throw std::runtime_error("Failed to create ZMQ context");
    }

    int socket_type;
    switch (type)
    {
        case SocketType::Publisher:
            socket_type = ZMQ_PUB;
            break;
        case SocketType::Subscriber:
            socket_type = ZMQ_SUB;
            break;
        case SocketType::Request:
            socket_type = ZMQ_REQ;
            break;
        case SocketType::Reply:
            socket_type = ZMQ_REP;
            break;
        case SocketType::Push:
            socket_type = ZMQ_PUSH;
            break;
        case SocketType::Pull:
            socket_type = ZMQ_PULL;
            break;
        case SocketType::Router:
            socket_type = ZMQ_ROUTER;
            break;
        case SocketType::Dealer:
            socket_type = ZMQ_DEALER;
            break;
        default: throw std::invalid_argument("Invalid socket type");
    }

    socket_ = zmq_socket(context_, socket_type);
    if (!socket_)
    {
        throw std::runtime_error("Failed to create ZMQ socket");
    }
    setTimeout(timeout_ms_);
    message_factory_ = MessageFactory();
}

MessagerBase::~MessagerBase()
{
    stop();
    if (socket_)
    {
        // Set linger to 0 to avoid blocking on close
        int linger = 0;
        zmq_setsockopt(socket_, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(socket_);
        socket_ = nullptr;
    }
}

MessagerBase::RequestStatus MessagerBase::createContext()
{
    if (!context_)
    {
        context_ = zmq_ctx_new();
        if (!context_)
        {
            return RequestStatus::Error;
        }
        context_ref_count_ = 1;
    }
    return RequestStatus::Success;
}

void MessagerBase::destroyContext()
{
    if (context_)
    {
        zmq_ctx_destroy(context_);
        context_ = nullptr;
    }
}

void MessagerBase::setTimeout(const int timeout_ms)
{
    timeout_ms_ = timeout_ms;
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_));
}

int MessagerBase::getTimeout() const
{
    return timeout_ms_;
}

MessagerBase::RequestStatus MessagerBase::bind(const std::string &endpoint)
{
    int rc = zmq_bind(socket_, endpoint.c_str());
    if (rc != 0)
    {
        return RequestStatus::Error;
    }
    return RequestStatus::Success;
}

MessagerBase::RequestStatus MessagerBase::connect(const std::string &endpoint)
{
    int rc = zmq_connect(socket_, endpoint.c_str());
    if (rc != 0)
    {
        return RequestStatus::Error;
    }
    return RequestStatus::Success;
}

MessagerBase::RequestStatus MessagerBase::disconnect(const std::string &endpoint)
{
    int rc = zmq_disconnect(socket_, endpoint.c_str());
    if (rc != 0)
    {
        return RequestStatus::Error;
    }
    return RequestStatus::Success;
}

MessagerBase::RequestStatus MessagerBase::sendMessage(const MessageInterface &message, const std::string &extra)
{
    if(!extra.empty())
    {
        // Send extra frame first
        zmq_msg_t extra_msg;
        zmq_msg_init_size(&extra_msg, extra.size());
        memcpy(zmq_msg_data(&extra_msg), extra.c_str(), extra.size());
        int rc = zmq_msg_send(&extra_msg, socket_, ZMQ_SNDMORE);
        if (rc < 0)
        {
            zmq_msg_close(&extra_msg);
            return RequestStatus::Error;
        }
        zmq_msg_close(&extra_msg);
    }
    // Pack the message into a ZMQ message
    zmq_msg_t msg;
    message.toZmqMessage(msg);
    // Send the message
    int rc = zmq_msg_send(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        return RequestStatus::Error;
    }
    zmq_msg_close(&msg);
    // Allow derived classes to perform actions after successful send
    onMessageSent();
    return RequestStatus::Success;
}

MessagerBase::RequestStatus MessagerBase::recvMessage(std::unique_ptr<MessageInterface> &message, int timeout_ms)
{
    // Set receive timeout
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    zmq_msg_t msg;
    zmq_msg_init(&msg);

    int rc = zmq_msg_recv(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        if (errno == EAGAIN)
        {
            return RequestStatus::Timeout;
        }
        return RequestStatus::Error;
    }

    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(socket_, ZMQ_RCVMORE, &more, &more_size);

    if(more)
    {
        // This is a multi-part message, receive the next part
        zmq_msg_close(&msg);
        zmq_msg_t next_msg;
        zmq_msg_init(&next_msg);
        rc = zmq_msg_recv(&next_msg, socket_, 0);
        if (rc < 0)
        {
            zmq_msg_close(&next_msg);
            if (errno == EAGAIN)
            {
                return RequestStatus::Timeout;
            }
            return RequestStatus::Error;
        }
        message = message_factory_.createFromZmqMessage(next_msg);
        zmq_msg_close(&next_msg);
    }
    else
    {
        // Single frame message
        message = message_factory_.createFromZmqMessage(msg);
        zmq_msg_close(&msg);
    }

    if(!message->isValid())
    {
        return RequestStatus::Error;
    }

    return RequestStatus::Success;
}

void MessagerBase::startReceiving(std::function<void(std::unique_ptr<MessageInterface>)> handler)
{
    message_handler_ = handler;
    setRunning(true);
    receive_thread_ = std::thread([this]() {
            receiveLoop();
        });
}

void MessagerBase::stop()
{
    running_.store(false);

    if (receive_thread_.joinable())
    {
        // Create a timeout mechanism for joining the thread
        std::atomic<bool> join_completed{false};

        // Start a thread to join the receive thread
        std::thread join_thread([this, &join_completed]() {
                                try {
                                    receive_thread_.join();
                                } catch (const std::exception &e) {
                                    // Ignore join exceptions during shutdown
                                }
                                join_completed.store(true);
                });

        // Wait up to 500ms for the thread to join
        auto timeout_ms = 500;
        auto wait_interval_ms = 10;
        auto max_iterations = timeout_ms / wait_interval_ms;

        for (int i = 0; i < max_iterations && !join_completed.load(); ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(wait_interval_ms));
        }

        if (join_completed.load())
        {
            // Thread joined successfully
            if (join_thread.joinable())
            {
                join_thread.join();
            }
        }
        else
        {
            // Thread didn't join in time, detach both threads
            if (socket_)
            {
                int linger = 0;
                zmq_setsockopt(socket_, ZMQ_LINGER, &linger, sizeof(linger));
                zmq_close(socket_);
                socket_ = nullptr;
            }

            join_thread.detach();
            receive_thread_.detach();
        }
    }
}

void MessagerBase::receiveLoop()
{
    while (running_.load())
    {
        // Quick check if we should stop before doing any ZMQ operations
        if (!running_.load())
        {
            break;
        }

        std::unique_ptr<MessageInterface> message;
        const RequestStatus status = recvMessage(message, timeout_ms_);
        if (status == RequestStatus::Timeout)
        {
            continue; // No message received, loop again
        }
        else if (status == RequestStatus::Error)
        {
            // Log error and continue
            logger_->error("Error receiving message in receive loop");
            continue;
        }
        else if(status == RequestStatus::Success && message)
        {
            // Handle the message asynchronously to avoid blocking the receive loop
            if (message_handler_)
            {
                // Process message in a separate thread to keep receive loop responsive
                std::thread([this, msg = std::move(message)]() mutable {
                        try {
                            message_handler_(std::move(msg));
                        } catch (const std::exception &e) {
                            logger_->error("Error in async message handler: %s", e.what());
                        }
                    }).detach();
            }
            onMessageReceived(); // Allow derived classes to perform additional actions
        }
    }
}
} // namespace PiTrac