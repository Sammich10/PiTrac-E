#ifndef GS_SYSTEM_MANAGER_H
#define GS_SYSTEM_MANAGER_H

#include "Application/Managers/ManagerBase/GSManagerBase.h"
#include "Common/System/System.h"

namespace PiTrac
{
class SystemManager : public GSManagerBase
{
  public:

    SystemManager();
    virtual ~SystemManager();

  protected:

    std::unique_ptr<GSMessagerBase> task_reg_receiver_;
    std::unique_ptr<GSMessagerBase> task_status_subscriber_;
    std::unique_ptr<GSMessagerBase> system_command_listener_;

    bool setupProcess() override;
    void cleanupProcess() override;
    bool execute() override;

  private:

    void taskRegistrationHandler
    (
        std::unique_ptr<MessageInterface> message
    );
    void handleExternalCommand
    (
        std::unique_ptr<MessageInterface> message
    );
// void handleModeChange(SystemMode_Type new_mode);

    typedef struct
    {
        std::string task_name;
        uint64_t task_pid;
    } RegisteredTask;

    std::list<RegisteredTask> registered_tasks_;

    std::unique_ptr<GSMessagerBase> mode_command_messager_;

    std::function<void(std::unique_ptr<MessageInterface>)> command_handler_;
};
} // namespace PiTrac

#endif // GS_SYSTEM_MANAGER_H