#ifndef SYSTEM_COMMAND_MSG_H
#define SYSTEM_COMMAND_MSG_H

#include "Infrastructure/Messaging/Messages/MessageBase.h"
#include "Common/System/System.h"
#include <string>
#include <variant>

namespace PiTrac
{

class SystemCommandMsg : public MessageBase
{
    public:

    enum class CommandID
    {
        SetMode,
        Calibrate,
    };
    
    struct SetModePayload
    {
        SystemMode_Type mode;
    };

    using CommandPayload = std::variant<SetModePayload>;

    SystemCommandMsg() = default;

    SystemCommandMsg(const CommandID &command, const CommandPayload &payload)
        : command_id_(command)
        , payload_(payload) 
        {}

    // MessageInterface implementation
    Message_Type getMessageType() const override
    {
        return Message_Type::SystemCommand;
    }

    void serialize(msgpack::sbuffer &buffer) const override;
    void deserialize(const char *data, size_t size) override;
    std::unique_ptr<MessageInterface> clone() const override;

    // Getters and setters
    const CommandID &getCommandID() const { return command_id_; }
    void setCommandID(const CommandID &command) { command_id_ = command; }

    const CommandPayload &getPayload() const { return payload_; }
    void setPayload(const CommandPayload &payload) { payload_ = payload; }

    std::string toString() const override;

    private:
    
    CommandID command_id_;
    CommandPayload payload_;

}; // class SystemCommandMsg

} // namespace PiTrac

#endif // SYSTEM_COMMAND_MSG_H