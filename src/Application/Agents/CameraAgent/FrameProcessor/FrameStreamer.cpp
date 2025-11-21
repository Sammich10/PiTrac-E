#include "Application/Agents/CameraAgent/FrameProcessor/FrameStreamer.h"
#include "Common/System/Endpoints.h"
namespace PiTrac
{
FrameStreamer::FrameStreamer
(
    std::shared_ptr<FrameBuffer> frame_buffer,
    const uint32_t camera_id
)
    : FrameProcessor(std::move(frame_buffer), camera_id)
    , frame_publisher_(std::make_unique<MessagerBase>(MessagerBase::SocketType::Publisher))
{
    name_ = "FrameStreamer_" + std::to_string(camera_id);
}

bool FrameStreamer::init()
{
    frame_publisher_->bind(Endpoints::getCameraStreamEndpoint(camera_id_));
    return true;
}

void FrameStreamer::processingLoop()
{
    while (!should_stop_)
    {
        cv::Mat frame;
        if (frame_buffer_->getFrame(frame))
        {
            CameraFrameMsg frame_msg;
            frame_msg.setCameraId(std::to_string(camera_id_));
            frame_msg.setFrame(frame);
            frame_msg.setFrameNumber(frame_counter_++);
            frame_msg.setCaptureTimestamp(std::chrono::system_clock::now());
            frame_msg.setJpegQuality(60);
            frame_publisher_->sendMessage(frame_msg);
        }
    }
}
}