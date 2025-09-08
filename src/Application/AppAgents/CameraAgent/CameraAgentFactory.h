#ifndef CAMERA_AGENT_FACTORY_H
#define CAMERA_AGENT_FACTORY_H

#include "Application/AppAgents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
class CameraAgentFactory
{
  public:
    static CameraAgent createCameraAgent(GSCameraInterface *const &camera,
                                         const std::string &endpoint)
    {
        return CameraAgent(camera, endpoint);
    }
};
} // namespace PiTrac

#endif // CAMERA_AGENT_FACTORY_H