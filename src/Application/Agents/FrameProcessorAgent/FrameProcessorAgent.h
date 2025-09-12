#ifndef FRAME_PROCESSOR_AGENT_H
#define FRAME_PROCESSOR_AGENT_H

#include "Application/Agents/AgentBase/GSAgentBase.h"
#include "Infrastructure/DataStructures/FrameBuffer.h"
#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include "Infrastructure/Messaging/Messages/GSCameraFrameRawMsg.h"
#include "Infrastructure/Messaging/Messages/GSCameraFrameMsg.h"
#include <opencv2/opencv.hpp>

namespace PiTrac
{

class FrameProcessorAgent : public GSAgentBase
{
  public:
    FrameProcessorAgent
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    );
    ~FrameProcessorAgent() override;

    bool setup() override;
    bool initialize() override;
    void execute() override;
    void cleanup() override;

  private:
    std::shared_ptr<FrameBuffer> frame_buffer_;
    std::atomic<bool> running_;
    std::unique_ptr<GSMessagerBase> frame_publisher_;
    uint32_t camera_id_;
    size_t frame_counter_;

    void processFrames();
};

} // namespace PiTrac

#endif // FRAME_PROCESSOR_AGENT_H