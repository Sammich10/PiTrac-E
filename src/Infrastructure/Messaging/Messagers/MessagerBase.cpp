#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{
void *MessagerBase::context_ = nullptr;

MessagerBase::MessagerBase(SocketType type)
    : socket_(nullptr)
    , socket_type_(type)
    , running_(false)
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
        zmq_close(socket_);
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
    zmq_msg_t msg;
    message.toZmqMessage(msg);

    int rc = zmq_msg_send(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        throw std::runtime_error("Failed to send message: " + std::string(zmq_strerror(errno)));
    }

    zmq_msg_close(&msg);
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
    running_ = true;
    receive_thread_ = std::thread([this]() {
            receiveLoop();
        });
}

void MessagerBase::stop()
{
    running_ = false;
    if (receive_thread_.joinable())
    {
        receive_thread_.join();
    }
}

MessageFactory message_factory_ = MessageFactory();

void MessagerBase::receiveLoop()
{
    while (running_)
    {
        zmq_msg_t msg;
        zmq_msg_init(&msg);

        int rc = zmq_msg_recv(&msg, socket_, 0);
        if (rc >= 0 && message_handler_)
        {
            printf("Received message of size %d\n", rc);
            // Create message from received ZMQ message
            try
            {
                std::unique_ptr<MessageInterface> message = message_factory_.createFromZmqMessage(msg);
                message_handler_(std::move(message));
            }
            catch (const std::exception &e)
            {
                zmq_msg_close(&msg);
                printf("Error creating message from ZMQ message: %s\n", e.what());
                continue; // Skip this message and continue
            }
        }
        else if (errno != EAGAIN)
        {
            zmq_msg_close(&msg);
            break; // Error occurred
        }
        zmq_msg_close(&msg);
    }
}

} // namespace PiTrac