#include "Infrastructure/Messaging/Messagers/MessageDealer.h"

namespace PiTrac
{
void MessageDealer::setIdentity(const std::string &identity)
{
    identity_ = identity;

    // Set ZMQ socket identity
    void *socket = getSocket();
    int rc = zmq_setsockopt(socket, ZMQ_IDENTITY, identity.c_str(), identity.size());
    if (rc != 0)
    {
        throw std::runtime_error("Failed to set dealer identity: " + std::string(zmq_strerror(errno)));
    }
}

std::optional<std::string> MessageDealer::getIdentity() const
{
    return identity_;
}

std::unique_ptr<MessageInterface> MessageDealer::sendRequestAndWaitForResponse(
    const MessageInterface &request,
    int timeout_ms)
{
    // Send the request
    sendMessage(request);

    // Wait for response
    void *socket = getSocket();

    // Set receive timeout
    zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    // Check for multi-part message (empty frame + message)
    zmq_msg_t first_msg;
    zmq_msg_init(&first_msg);

    int rc = zmq_msg_recv(&first_msg, socket, 0);
    if (rc < 0)
    {
        zmq_msg_close(&first_msg);
        if (errno == EAGAIN)
        {
            return nullptr; // Timeout
        }
        throw std::runtime_error("Failed to receive response: " + std::string(zmq_strerror(errno)));
    }

    // Check if this is a multi-part message
    int more;
    size_t more_size = sizeof(more);
    zmq_getsockopt(socket, ZMQ_RCVMORE, &more, &more_size);

    if (more)
    {
        // This was an empty frame, receive the actual message
        zmq_msg_close(&first_msg);
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        rc = zmq_msg_recv(&msg, socket, 0);

        if (rc < 0)
        {
            zmq_msg_close(&msg);
            if (errno == EAGAIN)
            {
                return nullptr; // Timeout
            }
            throw std::runtime_error("Failed to receive message part: " + std::string(zmq_strerror(errno)));
        }

        try {
            // Add safety checks for the ZMQ message
            size_t msg_size = zmq_msg_size(&msg);
            void *msg_data = zmq_msg_data(&msg);

            if (msg_size == 0)
            {
                zmq_msg_close(&msg);
                return nullptr;
            }

            if (msg_data == nullptr)
            {
                zmq_msg_close(&msg);
                return nullptr;
            }

            std::unique_ptr<MessageInterface> message = message_factory_.createFromZmqMessage(msg);
            zmq_msg_close(&msg);
            return message;
        } catch (const std::exception &e) {
            zmq_msg_close(&msg);
            throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
        } catch (...) {
            zmq_msg_close(&msg);
            throw std::runtime_error("Unknown exception during message parsing");
        }
    }
    else
    {
        // Single frame message
        try {
            std::unique_ptr<MessageInterface> message = message_factory_.createFromZmqMessage(first_msg);
            zmq_msg_close(&first_msg);
            return message;
        } catch (const std::exception &e) {
            zmq_msg_close(&first_msg);
            throw std::runtime_error("Failed to parse response: " + std::string(e.what()));
        }
    }
}

bool MessageDealer::isConnectedToRouter() const
{
    std::lock_guard<std::mutex> lock(connection_mutex_);
    return connected_to_router_;
}

void MessageDealer::onMessageReceived()
{
    updateConnectionStatus();
}

void MessageDealer::onMessageSent()
{
    updateConnectionStatus();
}

void MessageDealer::updateConnectionStatus()
{
    std::lock_guard<std::mutex> lock(connection_mutex_);
    connected_to_router_ = true; // Assume connected if we can send/receive
}
} // namespace PiTrac