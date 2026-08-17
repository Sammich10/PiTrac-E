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
    /**
     * @brief Constructs a MessageDealer object with an optional identity.
     */
    MessageDealer(const std::string &identity = "")
        : MessagerBase(SocketType::Dealer)
    {
        if (!identity.empty())
        {
            setIdentity(identity);
        }
    }

    /**
     * @brief Destructor for the MessageDealer class.
     */
    virtual ~MessageDealer() = default;

    /**
     * @brief Sends a message with an extra frame through the DEALER socket.
     * This method overrides the base class implementation to prepend an empty frame for ROUTER compatibility.
     * @param[in] message The message to be sent.
     * @param[in] extra The extra frame associated with the message.
     */
    RequestStatus sendMessage
    (
        const MessageInterface &message,
        const std::string &extra = ""
    ) final override;

    /**
     * @brief Gets the identity of the DEALER socket.
     * @return The identity of the DEALER socket, if set; otherwise, std::nullopt.
     */
    std::optional<std::string> getIdentity() const;

    /**
     * @brief Checks if the DEALER socket is connected to a ROUTER socket.
     * @return true if connected to a ROUTER socket, false otherwise.
     */
    bool isConnectedToRouter() const;

  protected:
    // Override to update connection status when messages are received
    void onMessageReceived() override;

    // Override to update connection status when messages are sent
    void onMessageSent() override;

  private:
    inline RequestStatus sendEmptyFrame();
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