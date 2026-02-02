#include "Application/Agents/CameraAgent/CameraAgent.h"

namespace PiTrac
{

class TeeAgent : public PiTrac::CameraAgent
{
  public:
    TeeAgent(const size_t camera_index);

    ~TeeAgent();

protected:
};

} // namespace PiTrac