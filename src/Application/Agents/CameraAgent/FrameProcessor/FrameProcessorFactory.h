#ifndef FRAME_PROCESSOR_FACTORY_H
#define FRAME_PROCESSOR_FACTORY_H

#include "Application/Agents/CameraAgent/FrameProcessor/FrameProcessor.h"

namespace PiTrac
{
class FrameProcessorFactory
{
  public:
    static std::unique_ptr<FrameProcessor> create
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    )
    {
        return std::make_unique<FrameProcessor>(std::move(frame_buffer), camera_id);
    }
};
}

#endif // FRAME_PROCESSOR_FACTORY_H