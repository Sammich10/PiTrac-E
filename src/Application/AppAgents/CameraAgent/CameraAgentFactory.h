#ifndef CAMERA_AGENT_FACTORY_H
#define CAMERA_AGENT_FACTORY_H

#include "Application/AppAgents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
class CameraAgentFactory
{
  public:
    static std::shared_ptr<CameraAgent> createCameraAgent
    (
        std::unique_ptr<GSCameraInterface> camera,
        const std::string &endpoint
    )
    {
        return std::make_shared<CameraAgent>(std::move(camera), endpoint);
    }
};
} // namespace PiTrac

#endif // CAMERA_AGENT_FACTORY_H