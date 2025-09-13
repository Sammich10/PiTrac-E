#include "Application/Agents/FrameProcessorAgent/FrameProcessorAgent.h"

namespace PiTrac
{
FrameProcessorAgent::FrameProcessorAgent(std::shared_ptr<FrameBuffer> frame_buffer, const uint32_t camera_id)
    : GSAgentBase("FrameProcessorAgent", AgentPriority::High),
    frame_buffer_(std::move(frame_buffer)),
    camera_id_(camera_id),
    frame_counter_(0),
    running_(false)
{
    agent_name_ = agent_name_ + " " + std::to_string(camera_id_);
    frame_publisher_ = std::make_unique<GSMessagerBase>(GSMessagerBase::SocketType::Publisher);
}

FrameProcessorAgent::~FrameProcessorAgent()
{
    cleanup();
    logInfo("FrameProcessorAgent destroyed: " + agent_name_);
}

bool FrameProcessorAgent::setup()
{
    logInfo("Setting up " + agent_name_ + " with buffer capacity: " + std::to_string(frame_buffer_->capacity()));
    switch(camera_id_)
    {
        case 0:
            frame_publisher_endpoint_ = "tcp://0.0.0.0:5555";
            break;
        case 1:
            frame_publisher_endpoint_ = "tcp://0.0.0.0:5556";
            break;
    }
    frame_publisher_->bind(frame_publisher_endpoint_);
    logInfo("FrameProcessorAgent bound to publisher endpoint: " + frame_publisher_endpoint_);
    return true;
}

bool FrameProcessorAgent::initialize()
{
    logInfo("Initializing " + agent_name_);
    return true;
}

void FrameProcessorAgent::execute()
{
    logInfo("FrameProcessorAgent execution started for: " + agent_name_);
    processFrames();
}

void FrameProcessorAgent::cleanup()
{
    logInfo("FrameProcessorAgent cleanup completed for: " + agent_name_);
    logInfo("Processed " + std::to_string(frame_counter_) + " frames.");
}

void FrameProcessorAgent::processFrames()
{
    while (!shouldStop())
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