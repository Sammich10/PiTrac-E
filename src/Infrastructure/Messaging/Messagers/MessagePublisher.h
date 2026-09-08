#ifndef __MESSAGE_PUBLISHER_H
#define __MESSAGE_PUBLISHER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{
class MessagePublisher : public MessagerBase
{
  public:
    MessagePublisher()
        : MessagerBase(SocketType::Publisher)
    {
    }

    virtual ~MessagePublisher() = default;
};
}; // namespace PiTrac

#endif // __MESSAGE_PUBLISHER_H