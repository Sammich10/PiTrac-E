#ifndef MESSAGE_ROUTER_H
#define MESSAGE_ROUTER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include <map>
#include <string>
#include <functional>

namespace PiTrac
{
class MessageRouter : public MessagerBase
{
  public:
    MessageRouter() : MessagerBase(SocketType::Router)
    {
    }

    virtual ~MessageRouter() = default;

    // Override base send method to require identity for router
    RequestStatus sendMessage
    (
        const MessageInterface &message,
        const std::string &extra = ""
    ) final override;

    // Override base receive method to return message with identity
    RequestStatus recvMessage
    (
        std::unique_ptr<MessageInterface> &message,
        int timeout_ms = 1000
    ) override;

    // Broadcast message to all known dealer identities
    void broadcastMessage
    (
        const MessageInterface &message
    );

    // Get list of known dealer identities
    std::vector<std::string> getConnectedDealers() const;

    // Check if a specific dealer is connected
    bool isDealerConnected
    (
        const std::string &identity
    ) const;

  private:

    std::function<void(std::unique_ptr<MessageInterface>)> legacy_message_handler_;

    // Track connected dealers
    mutable std::mutex dealers_mutex_;
    std::set<std::string> connected_dealers_;

    // Helper to add/update dealer connection
    void updateDealerConnection
    (
        const std::string &identity
    );
};
} // namespace PiTrac

#endif // MESSAGE_ROUTER_H