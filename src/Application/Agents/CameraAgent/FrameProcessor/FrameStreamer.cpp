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
    , frame_publisher_(std::make_unique<MessagerBase>(MessagerBase::SocketType::Push))
{
    name_ = "FrameStreamer_" + std::to_string(camera_id);
}

bool FrameStreamer::init()
{
    // Connect to frame collection endpoint
    frame_publisher_->connect(Endpoints::getFrameCollectionEndpoint());
    return true;
}

void FrameStreamer::processingLoop()
{
    running_ = true;
    printf("FrameStreamer processing loop started for camera %d\n", camera_id_);

    while (!should_stop_)
    {
        cv::Mat frame;
    }

    running_ = false;
    printf("FrameStreamer processing loop ended for camera %d\n", camera_id_);
}
}