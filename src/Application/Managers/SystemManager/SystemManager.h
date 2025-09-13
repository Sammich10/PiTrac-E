#ifndef GS_SYSTEM_MANAGER_H
#define GS_SYSTEM_MANAGER_H

#include "Infrastructure/Managers/GSManagerBase.h"

namespace PiTrac
{
/**
 * @class SystemManager
 * @brief Manages overall system operations and coordinates various components.
 *
 * The SystemManager is responsible for overseeing the entire system's
 *functionality,
 * coordinating among the system's active agents, triggering state transitions,
 *handling
 * events, and making high-level decisions based on system status and inputs.
 * It serves as the central control unit that ensures the system operates
 *correctly.
 */
class SystemManager : public GSManagerBase
{
};
} // namespace PiTrac

#endif // GS_SYSTEM_MANAGER_H