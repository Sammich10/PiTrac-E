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

    // Router-specific send method - sends to specific dealer identity
    void sendMessageToIdentity
    (
        const MessageInterface &message,
        const std::string &identity
    );

    // Override base send method to require identity for router
    void sendMessage(const MessageInterface &message) override
    {
        throw std::runtime_error("Router requires identity - use sendMessageToIdentity() instead");
    }

    // Router doesn't use topic-based sending
    void sendMessage(const MessageInterface &message, const std::string &topic) override
    {
        throw std::runtime_error("Router doesn't support topic sending - use sendMessageToIdentity() instead");
    }

    // Router-specific receiving with identity tracking
    void startReceivingWithIdentity
    (
        std::function<void(std::unique_ptr<MessagerBase::IdentityMessage>)> handler
    );

    // Override base receiving to use identity-aware version
    void startReceiving
    (
        std::function<void(std::unique_ptr<MessageInterface>)> handler
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

  protected:
    void receiveLoop() override;

  private:
    // Receive message with identity information
    std::unique_ptr<MessagerBase::IdentityMessage> receiveMessageWithIdentity();

    // Handler functions
    std::function<void(std::unique_ptr<MessagerBase::IdentityMessage>)> identity_message_handler_;
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