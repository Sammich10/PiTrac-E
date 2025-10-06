#ifndef TASKSTATUS_H
#define TASKSTATUS_H

namespace PiTrac
{

enum class TaskStatus
{
    NotStarted,
    Starting,
    Running,
    Paused,
    Stopping,
    Stopped,
    Timeout,
    Failed,
    Crashed
};

} // namespace PiTrac

#endif // TASKSTATUS_H