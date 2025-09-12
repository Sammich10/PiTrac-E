#ifndef CAMERA_AGENT_FACTORY_H
#define CAMERA_AGENT_FACTORY_H

#include "Application/Agents/CameraAgent/CameraAgent.h"

namespace PiTrac
{
class CameraAgentFactory
{
  public:
    static std::shared_ptr<CameraAgent> createCameraAgent
    (
        std::unique_ptr<GSCameraInterface> camera,
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    )
    {
        return std::make_shared<CameraAgent>(std::move(camera), frame_buffer, camera_id);
    }
};
} // namespace PiTrac

#endif // CAMERA_AGENT_FACTORY_H