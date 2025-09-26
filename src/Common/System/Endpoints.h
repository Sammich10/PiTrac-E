#ifndef ENDPOINTS_H
#define ENDPOINTS_H

#include <string>
#include <map>

namespace PiTrac
{
/**
 * @brief The Endpoints class provides static methods to
 * retrieve IPC endpoints used for inter-process communic
 * This is also where network endpoints will be defined.
 *
 * @note All endpoints should be defined here to maintain consistency.
 *
 * @todo At some point, these endpoints should be defined in a configuration
 * file
 * and loaded at runtime to allow for easier changes without recompilation.
 */
class Endpoints
{
  public:

    static const std::string getAgentTaskEndpoint()
    {
        return "ipc://agent_task_endpoint";
    }

    static const std::string getCameraStreamEndpoint(const size_t &cameraIndex)
    {
        switch(cameraIndex)
        {
            case 0:
                return "tcp://0.0.0.0:5555";
            case 1:
                return "tcp://0.0.0.0:5556";
            default:
                return "tcp://0.0.0.0:5555"; // Default to camera 0 if index is
                                             // out of range
        }
    }

    static const std::string getTaskRegistrationEndpoint()
    {
        return "ipc://task_registration_endpoint";
    }

    static const std::string getExternalCommandEndpoint()
    {
        return "tcp://0.0.0.0:6000";
    }

    static const std::string getTaskEndpoint()
    {
        return "ipc://task_endpoint";
    }

    static const std::string getTaskStatusEndpoint()
    {
        return "ipc://task_status_endpoint";
    }

    static const std::string getHostEndpoint()
    {
        return "tcp://192.168.86.32:6000";
    }

  private:

    static const std::string endpointsJsonFilePath_;
};
}

#endif // ENDPOINTS_H