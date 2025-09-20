#include "Application/Agents/CameraAgent/FrameProcessor/FrameProcessor.h"
#include "Common/System/Endpoints.h"
namespace PiTrac
{
FrameProcessor::FrameProcessor(std::shared_ptr<FrameBuffer> frame_buffer, const uint32_t camera_id)
    : frame_buffer_(std::move(frame_buffer)),
      camera_id_(camera_id),
      frame_counter_(0),
      should_stop_(false)
{
    name_ = name_ + " " + std::to_string(camera_id_);
    frame_publisher_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Publisher);
}

FrameProcessor::~FrameProcessor()
{
    frame_publisher_->stop();
}

bool FrameProcessor::init()
{
    frame_publisher_endpoint_ = Endpoints::getCameraStreamEndpoint(camera_id_);
    frame_publisher_->bind(frame_publisher_endpoint_);
    return true;
}

void FrameProcessor::streamFrames()
{
    if (processing_thread_.joinable())
    {
        logWarning("FrameProcessor is already processing frames! Stopping current processing.");
        should_stop_ = true;
        processing_thread_.join();
    }
    should_stop_ = false;
    processing_thread_ = std::thread(&FrameProcessor::streamingLoop, this);
    logInfo("FrameProcessor started streaming frames on " + frame_publisher_endpoint_);
}

void FrameProcessor::streamingLoop()
{
    while (!should_stop_)
    {
        cv::Mat frame;
        if (frame_buffer_->getFrame(frame))
        {
            GSCameraFrameMessage frame_msg;
            frame_msg.setCameraId(std::to_string(camera_id_));
            frame_msg.setFrame(frame);
            frame_msg.setFrameNumber(frame_counter_++);
            frame_msg.setCaptureTimestamp(std::chrono::system_clock::now());
            frame_msg.setJpegQuality(60);
            frame_publisher_->sendMessage(frame_msg);
        }
    }
}

} // namespace PiTrac