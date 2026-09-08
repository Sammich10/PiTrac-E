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

MessageDealer::RequestStatus MessageDealer::sendEmptyFrame()
{
    zmq_msg_t empty_frame;
    zmq_msg_init(&empty_frame);
    int rc = zmq_msg_send(&empty_frame, socket_, ZMQ_SNDMORE);
    if (rc < 0)
    {
        zmq_msg_close(&empty_frame);
        return RequestStatus::Error;
    }
    zmq_msg_close(&empty_frame);
    return RequestStatus::Success;
}

MessageDealer::RequestStatus MessageDealer::sendMessage(const MessageInterface &message, const std::string &extra)
{
    // For DEALER sockets, send empty frame first to be compatible with ROUTER
    // DEALER will automatically prepend identity: [identity][empty][message]
    RequestStatus empty_frame_status = sendEmptyFrame();
    if (empty_frame_status != RequestStatus::Success)
    {
        return empty_frame_status;
    }
    // Send the actual message (use the common base class implementation)
    return MessagerBase::sendMessage(message, extra);
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