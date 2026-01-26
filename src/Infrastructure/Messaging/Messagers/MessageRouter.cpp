#include "Infrastructure/Messaging/Messagers/MessageRouter.h"
#include <set>
#include <mutex>
#include <thread>

namespace PiTrac
{
void MessageRouter::sendMessageToIdentity(const MessageInterface &message, const std::string &identity)
{
    if (getSocketType() != SocketType::Router)
    {
        throw std::runtime_error("sendMessageToIdentity can only be used with ROUTER sockets");
    }

    void *socket = getSocket();

    // Send identity frame
    zmq_msg_t id_msg;
    zmq_msg_init_size(&id_msg, identity.size());
    memcpy(zmq_msg_data(&id_msg), identity.c_str(), identity.size());
    int rc = zmq_msg_send(&id_msg, socket, ZMQ_SNDMORE);
    if (rc < 0)
    {
        zmq_msg_close(&id_msg);
        throw std::runtime_error("Failed to send identity frame: " + std::string(zmq_strerror(errno)));
    }

    // Send empty delimiter frame
    zmq_msg_t delimiter;
    zmq_msg_init(&delimiter);
    rc = zmq_msg_send(&delimiter, socket, ZMQ_SNDMORE);
    if (rc < 0)
    {
        zmq_msg_close(&delimiter);
        throw std::runtime_error("Failed to send delimiter frame: " + std::string(zmq_strerror(errno)));
    }

    // Send message data
    zmq_msg_t msg;
    message.toZmqMessage(msg);
    rc = zmq_msg_send(&msg, socket, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        throw std::runtime_error("Failed to send message frame: " + std::string(zmq_strerror(errno)));
    }
    zmq_msg_close(&msg);
}

void MessageRouter::startReceivingWithIdentity(std::function<void(std::unique_ptr<MessagerBase::IdentityMessage>)> handler)
{
    identity_message_handler_ = handler;
    setRunning(true);

    // Use base class thread management - assign to the base class member
    receive_thread_ = std::thread([this]() {
            receiveLoop();
        });
}

void MessageRouter::startReceiving(std::function<void(std::unique_ptr<MessageInterface>)> handler)
{
    // Wrap the legacy handler to work with identity messages
    legacy_message_handler_ = handler;

    auto identity_wrapper = [this](std::unique_ptr<MessagerBase::IdentityMessage> identity_msg) {
                                if (legacy_message_handler_ && identity_msg)
                                {
                                    legacy_message_handler_(std::move(identity_msg->message));
                                }
                            };

    startReceivingWithIdentity(identity_wrapper);
}

void MessageRouter::broadcastMessage(const MessageInterface &message)
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);

    for (const std::string &dealer_identity : connected_dealers_)
    {
        try {
            sendMessageToIdentity(message, dealer_identity);
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

void MessageRouter::receiveLoop()
{
    while (isRunning())
    {
        try {
            auto identity_msg = receiveMessageWithIdentity();
            if (identity_msg)
            {
                // Update dealer connection tracking
                updateDealerConnection(identity_msg->sender_identity);

                // Handle the message asynchronously to avoid blocking the
                // receive loop
                if (identity_message_handler_)
                {
                    // Process message in a separate thread to keep receive loop
                    // responsive
                    std::thread([this, msg = std::move(identity_msg)]() mutable {
                            try {
                                identity_message_handler_(std::move(msg));
                            } catch (const std::exception &e) {
                                printf("Error in async message handler: %s\n", e.what());
                            }
                        }).detach();
                }
                else if (legacy_message_handler_)
                {
                    std::thread([this, msg = std::move(identity_msg)]() mutable {
                            try {
                                legacy_message_handler_(std::move(msg->message));
                            } catch (const std::exception &e) {
                                printf("Error in async legacy message handler: %s\n", e.what());
                            }
                        }).detach();
                }
            }
        } catch (const std::exception &e) {
            if (isRunning())   // Only log if we're still supposed to be running
            {
                printf("Error in router receive loop: %s\n", e.what());
            }
            break;
        }
    }
}

std::unique_ptr<MessagerBase::IdentityMessage> MessageRouter::receiveMessageWithIdentity()
{
    if (getSocketType() != SocketType::Router)
    {
        throw std::runtime_error("receiveMessageWithIdentity can only be used with ROUTER sockets");
    }

    void *socket = getSocket();
    // Set timeout
    int timeout = getTimeout();
    zmq_setsockopt(socket, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));

    // Receive identity frame
    zmq_msg_t id_msg;
    zmq_msg_init(&id_msg);
    int rc = zmq_msg_recv(&id_msg, socket, 0);
    if (rc < 0)
    {
        zmq_msg_close(&id_msg);
        if (errno == EAGAIN)
        {
            return nullptr; // Timeout
        }
        throw std::runtime_error("Failed to receive identity frame: " + std::string(zmq_strerror(errno)));
    }

    std::string identity(static_cast<char *>(zmq_msg_data(&id_msg)), zmq_msg_size(&id_msg));
    zmq_msg_close(&id_msg);

    // Receive delimiter frame (and discard it)
    zmq_msg_t delimiter;
    zmq_msg_init(&delimiter);
    rc = zmq_msg_recv(&delimiter, socket, 0);
    zmq_msg_close(&delimiter);
    if (rc < 0)
    {
        throw std::runtime_error("Failed to receive delimiter frame: " + std::string(zmq_strerror(errno)));
    }

    // Receive message frame
    zmq_msg_t msg;
    zmq_msg_init(&msg);
    rc = zmq_msg_recv(&msg, socket, 0);
    if (rc < 0)
    {
        zmq_msg_close(&msg);
        throw std::runtime_error("Failed to receive message frame: " + std::string(zmq_strerror(errno)));
    }

    // Create message from ZMQ message
    try {
        std::unique_ptr<MessageInterface> message = message_factory_.createFromZmqMessage(msg);
        auto identity_message = std::make_unique<MessagerBase::IdentityMessage>();
        identity_message->sender_identity = identity;
        identity_message->message = std::move(message);

        zmq_msg_close(&msg);
        return identity_message;
    } catch (const std::exception &e) {
        zmq_msg_close(&msg);
        throw std::runtime_error("Error creating message from ZMQ message: " + std::string(e.what()));
    }
}

void MessageRouter::updateDealerConnection(const std::string &identity)
{
    std::lock_guard<std::mutex> lock(dealers_mutex_);
    connected_dealers_.insert(identity);
}
} // namespace PiTrac