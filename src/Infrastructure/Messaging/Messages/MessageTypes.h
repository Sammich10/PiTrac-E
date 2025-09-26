#ifndef GS_MESSAGE_TYPES_H
#define GS_MESSAGE_TYPES_H

namespace PiTrac
{
enum class Message_Type
{
    CameraFrame,
    CameraFrameRaw,
    ChangeMode,
    RegisterTask,
    TaskStatus,
    Event,
    SystemCommand,
    AckMessage,
    // Future messages here...
};
} // namespace PiTrac

#endif // GS_MESSAGE_TYPES_H