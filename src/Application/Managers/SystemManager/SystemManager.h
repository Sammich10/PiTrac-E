#ifndef GS_SYSTEM_MANAGER_H
#define GS_SYSTEM_MANAGER_H

#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include "Common/System/SystemModes.h"

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
public:

SystemManager();
virtual ~SystemManager();
bool setup() override;
bool initialize() override;
void execute() override;
void cleanup() override;

private:

void handleCommand(std::unique_ptr<GSMessageInterface> message);
void handleModeChange(SystemMode_Type new_mode);

std::unique_ptr<GSMessagerBase> mode_command_messager_;

std::function<void(std::unique_ptr<GSMessageInterface>)> command_handler_;

};
} // namespace PiTrac

#endif // GS_SYSTEM_MANAGER_H