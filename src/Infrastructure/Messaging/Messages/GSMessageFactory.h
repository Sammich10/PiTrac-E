#ifndef MESSAGE_FACTORY_H
#define MESSAGE_FACTORY_H

#include "Infrastructure/Messaging/GSMessageInterface.h"
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
    void registerMessage(const std::string &type)
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

    std::unordered_map<std::string,
                       std::function<std::unique_ptr<GSMessageInterface>()> > creators_;
};
} // namespace PiTrac

#endif // MESSAGE_FACTORY_H