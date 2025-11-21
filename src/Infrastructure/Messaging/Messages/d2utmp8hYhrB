#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

#include "Infrastructure/Messaging/MessageInterface.h"
#include "Infrastructure/Messaging/Messages/MessageTypes.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace PiTrac
{
class MessageFactory
{
  public:
    MessageFactory();

    template<typename MessageType>
    void registerMessage(const Message_Type &type)
    {
        creators_[type] = []() {
                              return std::make_unique<MessageType>();
                          };
    }

    std::unique_ptr<MessageInterface> createFromZmqMessage
    (
        zmq_msg_t &msg
    );

  private:

    std::unordered_map<Message_Type,
                       std::function<std::unique_ptr<MessageInterface>()> > creators_;
    std::shared_ptr<GSLogger> logger_ = GSLogger::getInstance();
};
} // namespace PiTrac

#endif // MESSAGE_FACTORY_H