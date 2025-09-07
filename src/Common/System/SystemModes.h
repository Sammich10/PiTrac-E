#ifndef SYSTEM_MODES_H
#define SYSTEM_MODES_H

namespace PiTrac
{
enum class SystemMode
{
    Initializing,
    Idle,
    Calibrating,
    Waiting,
    CameraCapture,
    DataProcessing,
    ErrorHandling
};

enum class EventID
{
    AwaitingStrike,
    StrikeDetected,
    DataProcessed,
    ErrorOccurred
};
} // namespace PiTrac

#endif // SYSTEM_MODES_H