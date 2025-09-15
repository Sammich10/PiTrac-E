#ifndef GS_MESSAGE_TYPES_H
#define GS_MESSAGE_TYPES_H

namespace PiTrac
{
    enum class GSMessageType
    {
        CameraFrame,
        CameraFrameRaw,
        ChangeMode,
        // Future messages here...
    };
} // namespace PiTrac

#endif // GS_MESSAGE_TYPES_H