#include "Application/Agents/CameraAgent/FrameProcessor/FrameStreamer.h"
#include "Common/Utils/CodecUtils/CodecUtils.h"
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
    JpegCodec codec;
    while (!should_stop_)
    {
        cv::Mat frame;
        if (frame_buffer_->getFrame(frame))
        {
            CameraFrameMsg frame_msg(
                "Camera_" + std::to_string(camera_id_),
                frame_count_++,
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()),
                fps_,
                codec.encode(frame, CodecParams{ { {"quality", "90"} } }),
                { {"Codec", "JPEG"}, {"Quality", "90"} }
            );
            frame_publisher_->sendMessage(frame_msg);
        }
    }
}
}