#ifndef CAMERA_AGENT_TASK_H
#define CAMERA_AGENT_TASK_H

#include <string>
#include <memory>
#include "Infrastructure/AgentTask/GSAgentTask.h"
#include "Application/AppAgents/CameraAgent/CameraAgentFactory.h"

namespace PiTrac
{
class CameraAgentTask : public GSAgentTask
{
  public:

    CameraAgentTask();
    ~CameraAgentTask() override = default;

    void configureAgents() override;

  protected:

    bool preAgentStartHook() override;
    void postAgentStartHook() override;
    void onAgentFailedHook
    (
        std::shared_ptr<GSAgentBase> agent
    ) override;
    void agentMonitoringLoopHook() override;
    bool setupProcess() override;
    void preStopHook() override;

  private:

    std::array<std::shared_ptr<CameraAgent>, 2> camera_agents_;
    std::shared_ptr<GSLogger> logger_;
}; // class CameraAgentTask
} // namespace PiTrac

#endif // CAMERA_AGENT_TASK_H