#ifndef FRAME_PROCESSOR_FACTORY_H
#define FRAME_PROCESSOR_FACTORY_H

#include "Application/Agents/CameraAgent/FrameProcessor/FrameProcessor.h"
#include "Application/Agents/CameraAgent/FrameProcessor/FrameStreamer.h"
#include <memory>
#include <unordered_map>
#include <functional>

namespace PiTrac
{
/**
 * @class FrameProcessorFactory
 * @brief Factory class to create FrameProcessor instances based on type.
 */
class FrameProcessorFactory
{
  public:


    static std::unique_ptr<FrameProcessor> create
    (
        FrameProcessor::ProcessorType type,
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    )
    {
        switch (type)
        {
            case FrameProcessor::ProcessorType::STREAMING:
                return std::make_unique<FrameStreamer>(std::move(frame_buffer), camera_id);
            default:
                return nullptr;
        }
    }

  private:
    FrameProcessorFactory() = delete;
    ~FrameProcessorFactory() = delete;
};
}

#endif // FRAME_PROCESSOR_FACTORY_H