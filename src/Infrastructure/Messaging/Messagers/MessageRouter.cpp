#include "Infrastructure/Messaging/Messagers/MessageRouter.h"
#include <set>
#include <mutex>
#include <thread>

namespace PiTrac
{
MessagerBase::RequestStatus MessageRouter::sendMessage(const MessageInterface &message, const std::string &extra)
{
    if (extra.empty())
    {
        return RequestStatus::Error; // Extra information (dealer identity) is required for router
    }
    // Send identity frame
    zmq_msg_t id_msg;
    zmq_msg_init_size(&id_msg, extra.size());
    memcpy(zmq_msg_data(&id_msg), extra.c_str(), extra.size());
    int rc = zmq_msg_send(&id_msg, socket_, ZMQ_SNDMORE);
    if (rc < 0)
    {
        zmq_msg_close(&id_msg);
        return RequestStatus::Error;
    }
    // Send empty delimiter frame
    zmq_msg_t delimiter;
    zmq_msg_init(&delimiter);
    rc = zmq_msg_send(&delimiter, socket_, ZMQ_SNDMORE);
    if (rc < 0)
    {
        zmq_msg_close(&delimiter);
        return RequestStatus::Error;
    }
    // Send message data
    zmq_msg_t msg;
    message.toZmqMessage(msg);
    rc = zmq_msg_send(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        return RequestStatus::Error;
    }
    zmq_msg_close(&msg);
    return RequestStatus::Success;
}

MessagerBase::RequestStatus MessageRouter::recvMessage(std::unique_ptr<MessageInterface> &message, int timeout_ms)
{
    // Set timeout
    zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    // Receive identity frame
    zmq_msg_t id_msg;
    zmq_msg_init(&id_msg);
    int rc = zmq_msg_recv(&id_msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&id_msg);
        if (errno == EAGAIN)
        {
            return RequestStatus::Timeout; // Timeout
        }
        return RequestStatus::Error; // Other error
    }

    std::string identity(static_cast<char *>(zmq_msg_data(&id_msg)), zmq_msg_size(&id_msg));
    zmq_msg_close(&id_msg);

    // Receive delimiter frame (and discard it)
    zmq_msg_t delimiter;
    zmq_msg_init(&delimiter);
    rc = zmq_msg_recv(&delimiter, socket_, 0);
    zmq_msg_close(&delimiter);
    if (rc < 0)
    {
        return RequestStatus::Error; // Other error
    }

    // Receive message frame
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    rc = zmq_msg_recv(&msg, socket_, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        return RequestStatus::Error; // Other error
    }

    // Create message from ZMQ message
    message = message_factory_.createFromZmqMessage(msg);
    message->setIdentity(identity); // Store the sender's identity in the message
    zmq_msg_close(&msg);

    if(!message->isValid())
    {
        return RequestStatus::Error;
    }
    return RequestStatus::Success;
}

void MessageRouter::broadcastMessage(const MessageInterface &message)
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);

    for (const std::string &dealer_identity : connected_dealers_)
    {
        try {
            sendMessage(message, dealer_identity);
        } catch (const std::exception &e) {
            // Log error but continue broadcasting to other dealers
            printf("Failed to broadcast to dealer [%s]: %s\n", dealer_identity.c_str(), e.what());
        }
    }
}

std::vector<std::string> MessageRouter::getConnectedDealers() const
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);
    return std::vector<std::string>(connected_dealers_.begin(), connected_dealers_.end());
}

bool MessageRouter::isDealerConnected(const std::string &identity) const
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);
    return connected_dealers_.find(identity) != connected_dealers_.end();
}

void MessageRouter::updateDealerConnection(const std::string &identity)
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);
    connected_dealers_.insert(identity);
}
} // namespace PiTrac