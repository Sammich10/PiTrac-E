#ifndef GS_MESSAGE_BASE_H
#define GS_MESSAGE_BASE_H

#include <string>
#include <chrono>
#include <json/json.h>
#include <msgpack.hpp>

namespace PiTrac
{

class GSMessageBase
{
public:
    GSMessageBase() = default;
    virtual ~GSMessageBase() = default;

    virtual void toJson(Json::Value& json) const = 0;
    virtual void fromJson(const Json::Value& json) = 0;
    virtual void serialize(std::string& out) const = 0;
    virtual void deserialize(const std::string& in) = 0;
    virtual std::string getMessageType() const = 0;
    virtual std::chrono::system_clock::time_point getTimestamp() const = 0;
};

} // namespace PiTrac

#endif // GS_MESSAGE_BASE_H6