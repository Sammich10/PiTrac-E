#ifndef APP_LAUNCHER_H
#define APP_LAUNCHER_H

#include <string>
#include <memory>
#include "Infrastructure/Messaging/GSMessagerBase.h"

namespace PiTrac
{
class AppLauncher
{
  public:
    AppLauncher();
    ~AppLauncher();

    void launchApp
    (
        const std::string &appName
    );
    void closeApp
    (
        const std::string &appName
    );

  private:
    std::unique_ptr<GSMessagerBase> messager_;
};
} // namespace PiTrac

#endif // APP_LAUNCHER_H