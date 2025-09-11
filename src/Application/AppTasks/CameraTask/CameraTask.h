#ifndef CAMERA_AGENT_TASK_H
#define CAMERA_AGENT_TASK_H

#include <string>
#include <memory>
#include "Infrastructure/AgentTask/GSAgentTask.h"
#include "Application/AppAgents/CameraAgent/CameraAgentFactory.h"
#include "Application/AppAgents/FrameProcessorAgent/FrameProcessorAgentFactory.h"

namespace PiTrac
{
class CameraTask : public GSAgentTask
{
  public:

    CameraTask();
    ~CameraTask() override = default;

    void configureAgents() override;

  protected:

    bool preAgentStartHook() override;
    void preStopHook() override;

  private:

    void checkForStrayIPAProcesses();

    std::array<std::shared_ptr<CameraAgent>, 2> camera_agents_;
    std::shared_ptr<GSLogger> logger_;
    std::array<std::string, 2> camera_endpoints_;
    std::array<std::string, 2> camera_ids_;
    std::array<std::shared_ptr<FrameBuffer>, 2> frame_buffers_;
}; // class CameraTask
} // namespace PiTrac

#endif // CAMERA_AGENT_TASK_H