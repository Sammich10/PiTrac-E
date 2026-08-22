#ifndef __MESSAGE_PUSHER_H
#define __MESSAGE_PUSHER_H 

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"

namespace PiTrac
{

    class MessagePusher : public MessagerBase
    {
      public:
        MessagePusher()
            : MessagerBase(SocketType::Push)
        {
        }

        virtual ~MessagePusher() = default;
    };

}; // namespace PiTrac

#endif // __MESSAGE_PUSHER_H