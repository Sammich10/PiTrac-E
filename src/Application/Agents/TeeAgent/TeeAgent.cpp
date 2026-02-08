#include "Application/Agents/TeeAgent/TeeAgent.h"

namespace PiTrac
{
TeeAgent::TeeAgent(const size_t camera_index)
    : PiTrac::CameraAgent(camera_index, "TeeAgent")
{
}

TeeAgent::~TeeAgent()
{
}
} // namespace PiTrac