#include "Application/AppAgents/FrameProcessorAgent/FrameProcessorAgent.h"

namespace PiTrac
{

FrameProcessorAgent::FrameProcessorAgent(std::shared_ptr<FrameBuffer> frame_buffer, const uint32_t camera_id)
    : GSAgentBase("FrameProcessorAgent", AgentPriority::High),
    frame_buffer_(std::move(frame_buffer)),
    camera_id_(camera_id),
    frame_counter_(0),
    running_(false)
{
    agent_name_ = agent_name_ + "_" + std::to_string(camera_id_);
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
    frame_publisher_->bind("ipc://frame_publisher_" + std::to_string(camera_id_));
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
            frame_publisher_->sendMessage(frame_msg);
        }
    }
}

} // namespace PiTrac