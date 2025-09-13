#ifndef CAMERA_AGENT_TASK_H
#define CAMERA_AGENT_TASK_H

#include <string>
#include <memory>
#include <libcamera/camera_manager.h>
#include "Infrastructure/AgentTask/GSAgentTask.h"
#include "Application/Agents/CameraAgent/CameraAgentFactory.h"
#include "Application/Agents/FrameProcessorAgent/FrameProcessorAgentFactory.h"

namespace PiTrac
{
class CameraTask : public GSAgentTask
{
  public:

    CameraTask
    (
      const size_t camera_index,
      const size_t frame_buffer_size
    );
    ~CameraTask() override = default;

    void configureAgents() override;

  protected:
    bool setupProcess() override;
    bool preAgentStartHook() override;
    void cleanupProcess() override;

  private:

    void checkForStrayIPAProcesses();

    std::shared_ptr<libcamera::CameraManager> cameraManager_;
    std::shared_ptr<GSLogger> logger_;
    size_t camera_index_;
    std::string camera_id_;
    std::shared_ptr<FrameBuffer> frame_buffer_;
    size_t frame_buffer_size_;
}; // class CameraTask
} // namespace PiTrac

#endif // CAMERA_AGENT_TASK_H