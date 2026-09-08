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
};
}; // namespace PiTrac

#endif // MESSAGE_REPLIER_H