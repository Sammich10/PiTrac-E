#ifndef __MESSAGE_REQUESTER_H
#define __MESSAGE_REQUESTER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{
class MessageRequester : public MessagerBase
{
  public:
    MessageRequester()
        : MessagerBase(SocketType::Request)
    {
    }

    virtual ~MessageRequester() = default;
};
}; // namespace PiTrac

#endif // __MESSAGE_REQUESTER_H
