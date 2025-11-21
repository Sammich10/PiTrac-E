#ifndef MESSAGER_FACTORY_H
#define MESSAGER_FACTORY_H

#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messagers/MessageRouter.h"
#include "Infrastructure/Messaging/Messagers/MessageDealer.h"
#include <memory>

namespace PiTrac
{
class MessagerFactory
{
  public:
    // Create appropriate messager based on socket type
    static std::unique_ptr<MessagerBase> createMessager(MessagerBase::SocketType type)
    {
        switch (type)
        {
            case MessagerBase::SocketType::Router:
                return std::make_unique<MessageRouter>();
            case MessagerBase::SocketType::Dealer:
                return std::make_unique<MessageDealer>();
            default:
                return std::make_unique<MessagerBase>(type);
        }
    }

    // Convenience methods for specific types
    static std::unique_ptr<MessageRouter> createRouter()
    {
        return std::make_unique<MessageRouter>();
    }

    static std::unique_ptr<MessageDealer> createDealer(const std::string &identity = "")
    {
        auto dealer = std::make_unique<MessageDealer>();
        if (!identity.empty())
        {
            dealer->setIdentity(identity);
        }
        return dealer;
    }
};
} // namespace PiTrac

#endif // MESSAGER_FACTORY_H