#ifndef __PITRAC_TASKNAMES_H__
#define __PITRAC_TASKNAMES_H__

#include <string>
#include <unordered_map>

namespace PiTrac
{
enum class TaskNames
{
    SystemManager,
    FlightAgent,
    TeeAgent,
    MAX_TASK_NAME
};

/**
 * @brief Utility class for TaskNames conversion and validation
 */
class TaskNameUtils
{
  public:
    // Convert enum to string
    static std::string toString(TaskNames task)
    {
        switch(task)
        {
            case TaskNames::SystemManager: return "SystemManager";
            case TaskNames::FlightAgent: return "FlightAgent";
            case TaskNames::TeeAgent: return "TeeAgent";
            default: return "Unknown";
        }
    }

    // Convert string to enum
    static TaskNames fromString(const std::string &task_name)
    {
        static const std::unordered_map<std::string, TaskNames> name_map = {
            {"SystemManager", TaskNames::SystemManager},
            {"FlightAgent", TaskNames::FlightAgent},
            {"TeeAgent", TaskNames::TeeAgent}
        };

        auto it = name_map.find(task_name);
        return (it != name_map.end()) ? it->second : TaskNames::MAX_TASK_NAME;
    }

    // Validate task name
    static bool isValidTaskName(const std::string &task_name)
    {
        return fromString(task_name) != TaskNames::MAX_TASK_NAME;
    }

    // Get all valid task names as strings
    static std::vector<std::string> getAllTaskNames()
    {
        return {
            toString(TaskNames::SystemManager),
            toString(TaskNames::FlightAgent),
            toString(TaskNames::TeeAgent)
        };
    }
};
}

#endif // __PITRAC_TASKNAMES_H__