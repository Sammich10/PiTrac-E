#ifndef FRAME_PROCESSOR_AGENT_H
#define FRAME_PROCESSOR_AGENT_H

#include "Application/Agents/AgentBase/AgentBase.h"
#include "Infrastructure/DataStructures/FrameBuffer.h"
#include "Infrastructure/Messaging/Messagers/MessagerBase.h"
#include "Infrastructure/Messaging/Messages/External/CameraFrameMsg.h"
#include <opencv2/opencv.hpp>
#include <thread>

namespace PiTrac
{
/**
 * @class FrameProcessor
 * @brief Agent responsible for processing frames from the camera.
 *
 * This agent retrieves raw frames from a shared FrameBuffer, processes them
 * based on the system mode and applicable processing pipeline, can publish
 * frames to other system components or external systems via ZeroMQ.
 */
class FrameProcessor
{
  public:
    FrameProcessor
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    );
    ~FrameProcessor();

    bool init();
    bool stop()
    {
        should_stop_ = true; if (processing_thread_.joinable())
        {
            processing_thread_.join();
        }
        return true;
    }

    bool isRunning() const
    {
        return running_;
    }

    void streamFrames();

  private:

    void streamingLoop();
    std::shared_ptr<FrameBuffer> frame_buffer_;
    std::unique_ptr<MessagerBase> frame_publisher_;
    uint32_t camera_id_;
    size_t frame_counter_;
    std::string name_;
    std::thread processing_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
};
} // namespace PiTrac

#endif // FRAME_PROCESSOR_AGENT_H