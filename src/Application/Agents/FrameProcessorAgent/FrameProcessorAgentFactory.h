#ifndef FRAME_PROCESSOR_FACTORY_H
#define FRAME_PROCESSOR_FACTORY_H

#include "Application/Agents/FrameProcessorAgent/FrameProcessorAgent.h"

namespace PiTrac
{

class FrameProcessorAgentFactory
{
public:
    static std::unique_ptr<FrameProcessorAgent> create
    (
        std::shared_ptr<FrameBuffer> frame_buffer,
        const uint32_t camera_id
    )
    {
        return std::make_unique<FrameProcessorAgent>(std::move(frame_buffer), camera_id);
    }
};

}

#endif // FRAME_PROCESSOR_FACTORY_H