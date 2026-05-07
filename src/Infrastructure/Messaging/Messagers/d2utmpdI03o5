#ifndef MESSAGE_DEALER_H
#define MESSAGE_DEALER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include <string>
#include <functional>
#include <optional>

namespace PiTrac
{
class MessageDealer : public MessagerBase
{
  public:
    MessageDealer(const std::string &identity = "")
        : MessagerBase(SocketType::Dealer)
    {
        if (!identity.empty())
        {
            setIdentity(identity);
        }
    }

    virtual ~MessageDealer() = default;

    // Get dealer identity (if set)
    std::optional<std::string> getIdentity() const;

    // Request-response pattern for dealers
    std::unique_ptr<MessageInterface> sendRequestAndWaitForResponse
    (
        const MessageInterface &request,
        int timeout_ms = 5000
    );

    // Check connection status to router
    bool isConnectedToRouter() const;

  protected:
    // Override to update connection status when messages are received
    void onMessageReceived() override;

    // Override to update connection status when messages are sent
    void onMessageSent() override;

  private:
    // Set dealer identity (optional - ZMQ will auto-generate if not set)
    void setIdentity
    (
        const std::string &identity
    );

    std::optional<std::string> identity_;
    std::function<void(std::unique_ptr<MessageInterface>)> message_handler_;

    mutable std::mutex connection_mutex_;
    bool connected_to_router_ = false;

    // Helper to check and update connection status
    void updateConnectionStatus();
};
} // namespace PiTrac

#endif // MESSAGE_DEALER_H