#ifndef FRAME_PROCESSOR_AGENT_H
#define FRAME_PROCESSOR_AGENT_H

#include "Infrastructure/DataStructures/FrameBuffer.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>

namespace PiTrac
{
/**
 * @class FrameProcessor
 * @brief A frame processor is an abstract class used by camera agent to do
 * something with the frames captured and placed on the FrameBuffer. It defines
 * the high level iterface for the camera agent to use to process frames.
 *
 */
class FrameProcessor
{
  public:
    enum ProcessorType
    {
        STREAMING,
        DETECTION,
        TRACKING,
        CALIBRATION,
        // Future processor types
        INVALID
    };

    FrameProcessor
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    )  
        : frame_buffer_(std::move(frame_buffer)),
        camera_id_(camera_id),
        frame_counter_(0),
        should_stop_(false),
        running_(false),
        type_(ProcessorType::INVALID)
    {
    }

    ~FrameProcessor()
    {
        stop();
    }

    virtual bool init() = 0;

    bool start()
    {
        should_stop_ = false;
        processing_thread_ = std::thread(&FrameProcessor::processingLoop, this);
        return true;
    }

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

  protected:

    virtual void processingLoop() = 0;
    std::shared_ptr<FrameBuffer> frame_buffer_;
    uint32_t camera_id_;
    size_t frame_counter_;
    std::string name_;
    std::thread processing_thread_;
    std::atomic<bool> running_;
    std::atomic<bool> should_stop_;
    ProcessorType type_;
};
} // namespace PiTrac

#endif // FRAME_PROCESSOR_AGENT_H