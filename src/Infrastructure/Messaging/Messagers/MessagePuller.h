#ifndef __MESSAGE_PULLER_H
#define __MESSAGE_PULLER_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{

    class MessagePuller : public MessagerBase
    {
      public:
        MessagePuller()
            : MessagerBase(SocketType::Pull)
        {
        }

        virtual ~MessagePuller() = default;
    };

}; // namespace PiTrac

#endif // __MESSAGE_PULLER_H