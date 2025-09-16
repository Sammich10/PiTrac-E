#ifndef SYSTEM_TASK_H
#define SYSTEM_TASK_H

#include "Infrastructure/ManagerTask/GSManagerTask.h"
#include "Application/Managers/SystemManager/SystemManager.h"

namespace PiTrac
{

class SystemTask : public GSManagerTask
{
  public:
    SystemTask();
    ~SystemTask() override = default;

    void configureManagers();

  protected:
    bool setupProcess() override;
    bool preManagerStartHook() override;
    void cleanupProcess() override;

  private:
    // Add private members and methods as needed
}; // class SystemTask

} // namespace PiTrac

#endif // SYSTEM_TASK_H