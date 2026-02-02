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

void MessagerBase::setTimeout(const int timeout_ms)
{
    timeout_ms_ = timeout_ms;
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms_, sizeof(timeout_ms_));
}

int MessagerBase::getTimeout() const
{
    return timeout_ms_;
}

void MessagerBase::bind(const std::string &endpoint)
{
    int rc = zmq_bind(socket_, endpoint.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to bind to " + endpoint + ": " + zmq_strerror(errno));
    }
}

void MessagerBase::connect(const std::string &endpoint)
{
    int rc = zmq_connect(socket_, endpoint.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to connect to " + endpoint + ": " +
                                 zmq_strerror(errno));
    }
}

void MessagerBase::disconnect(const std::string &endpoint)
{
    int rc = zmq_disconnect(socket_, endpoint.c_str());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to disconnect from " + endpoint + ": " +
                                 zmq_strerror(errno));
    }
}

void MessagerBase::subscribe(const std::string &topic)
{
    int rc = zmq_setsockopt(socket_, ZMQ_SUBSCRIBE, topic.c_str(), topic.length());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to subscribe to topic: " +
                                 std::string(zmq_strerror(errno)));
    }
}

void MessagerBase::sendMessage(const MessageInterface &message)
{
    // For DEALER sockets, send empty frame first to be compatible with ROUTER
    // DEALER will automatically prepend identity: [identity][empty][message]
    if (socket_type_ == SocketType::Dealer)
    {
        zmq_msg_t empty_frame;
        zmq_msg_init(&empty_frame);
        int rc = zmq_msg_send(&empty_frame, socket_, ZMQ_SNDMORE);
        if (rc < 0)
        {
            zmq_msg_close(&empty_frame);
            throw std::runtime_error("Failed to send empty frame: " + std::string(zmq_strerror(errno)));
        }
        zmq_msg_close(&empty_frame);
    }

    // Send the actual message
    zmq_msg_t msg;
    message.toZmqMessage(msg);

    int rc = zmq_msg_send(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        throw std::runtime_error("Failed to send message: " + std::string(zmq_strerror(errno)));
    }

    zmq_msg_close(&msg);

    // Allow derived classes to perform actions after successful send
    onMessageSent();
}

void MessagerBase::sendMessage(const MessageInterface &message, const std::string &topic)
{
    // Send topic frame first
    zmq_msg_t topic_msg;
    zmq_msg_init_size(&topic_msg, topic.size());
    memcpy(zmq_msg_data(&topic_msg), topic.c_str(), topic.size());
    zmq_msg_send(&topic_msg, socket_, ZMQ_SNDMORE);
    zmq_msg_close(&topic_msg);

    // Send message frame
    sendMessage(message);
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

MessageFactory message_factory_ = MessageFactory();

void MessagerBase::receiveLoop()
{
    while (running_.load())
    {
        // Quick check if we should stop before doing any ZMQ operations
        if (!running_.load())
        {
            break;
        }

        zmq_msg_t msg;
        int init_rc = zmq_msg_init(&msg);
        if (init_rc != 0)
        {
            break;
        }

        // Use non-blocking receive with timeout handled by socket option
        int rc = zmq_msg_recv(&msg, socket_, ZMQ_DONTWAIT);

        if (rc >= 0 && message_handler_)
        {
            // Check if this is a multi-part message (DEALER receiving from
            // ROUTER)
            int more;
            size_t more_size = sizeof(more);
            zmq_getsockopt(socket_, ZMQ_RCVMORE, &more, &more_size);

            std::unique_ptr<MessageInterface> message;

            if (more)
            {
                // This was an empty frame from ROUTER, receive the actual
                // message
                zmq_msg_close(&msg);

                zmq_msg_t actual_msg;
                zmq_msg_init(&actual_msg);
                int second_rc = zmq_msg_recv(&actual_msg, socket_, ZMQ_DONTWAIT);

                if (second_rc >= 0)
                {
                    try
                    {
                        message = message_factory_.createFromZmqMessage(actual_msg);
                    }
                    catch (const std::exception &e)
                    {
                        zmq_msg_close(&actual_msg);
                        continue;
                    }
                }
                else
                {
                    zmq_msg_close(&actual_msg);
                    continue;
                }
                zmq_msg_close(&actual_msg);
            }
            else
            {
                // This is a single frame message
                try
                {
                    message = message_factory_.createFromZmqMessage(msg);
                }
                catch (const std::exception &e)
                {
                    zmq_msg_close(&msg);
                    continue;
                }
            }

            if (message)
            {
                message_handler_(std::move(message));
                onMessageReceived(); // Allow derived classes to perform
                                     // additional actions
            }
        }
        else if (errno == EAGAIN)
        {
            // No message available, sleep briefly to avoid busy waiting
            // But check running flag more frequently
            for (int i = 0; i < 10 && running_.load(); ++i)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        else if (errno == ETERM)
        {
            // Context was terminated
            zmq_msg_close(&msg);
            break;
        }
        else if (errno == ENOTSOCK)
        {
            zmq_msg_close(&msg);
            break;
        }
        else if (errno == EINTR)
        {
            zmq_msg_close(&msg);
            continue;
        }
        else
        {
            // Other error occurred
            zmq_msg_close(&msg);
            break;
        }

        int close_rc = zmq_msg_close(&msg);
        if (close_rc != 0)
        {
            // Message close failed, but continue
        }
    }
}
} // namespace PiTrac