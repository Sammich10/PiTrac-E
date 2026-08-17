#ifndef MESSAGE_REPLIER_H
#define MESSAGE_REPLIER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{

    class MessageReplier : public MessagerBase
    {
      public:
        MessageReplier()
            : MessagerBase(SocketType::Reply)
        {
        }

        virtual ~MessageReplier() = default;

        // Override base send method to require identity for reply
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
    };

}; // namespace PiTrac

#endif // MESSAGE_REPLIER_H