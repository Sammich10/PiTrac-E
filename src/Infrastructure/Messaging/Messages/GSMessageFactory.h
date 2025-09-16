#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

#include "Infrastructure/Messaging/GSMessageInterface.h"
#include "Infrastructure/Messaging/Messages/GSMessageTypes.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace PiTrac
{
class GSMessageFactory
{
  public:
    GSMessageFactory();

    template<typename MessageType>
    void registerMessage(const GSMessageType &type)
    {
        creators_[type] = []() {
                              return std::make_unique<MessageType>();
                          };
    }

    std::unique_ptr<GSMessageInterface> createFromZmqMessage
    (
        zmq_msg_t &msg
    );

  private:

    std::unordered_map<GSMessageType,
                       std::function<std::unique_ptr<GSMessageInterface>()> > creators_;
    std::shared_ptr<GSLogger> logger_ = GSLogger::getInstance();
};
} // namespace PiTrac

#endif // MESSAGE_FACTORY_H