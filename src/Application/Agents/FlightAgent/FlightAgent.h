#include "Application/Agents/CameraAgent/CameraAgent.h"

namespace PiTrac
{

class FlightAgent : public PiTrac::CameraAgent
{
public:
    FlightAgent(const size_t camera_index);

    ~FlightAgent();

protected:
};

} // namespace PiTrac