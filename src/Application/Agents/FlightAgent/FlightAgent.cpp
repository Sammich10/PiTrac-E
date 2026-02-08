#include "Application/Agents/FlightAgent/FlightAgent.h"

namespace PiTrac
{
FlightAgent::FlightAgent(const size_t camera_index)
    : PiTrac::CameraAgent(camera_index, "FlightAgent")
{
}

FlightAgent::~FlightAgent()
{
}
} // namespace PiTrac
