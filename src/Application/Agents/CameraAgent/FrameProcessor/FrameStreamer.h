#ifndef FRAME_STREAMER_H
#define FRAME_STREAMER_H

#include "Application/Agents/CameraAgent/FrameProcessor/FrameProcessor.h"
#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messages/External/CameraFrameMsg.h"

namespace PiTrac
{
class FrameStreamer : public FrameProcessor
{
  public:
    FrameStreamer
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    );

    ~FrameStreamer();

    bool init() override;

  private:
    void processingLoop() override;
    std::unique_ptr<MessagerBase> frame_publisher_;
};
}

#endif // FRAME_STREAMER_H