/**
 * Start Application
 *
 * This file is part of the PiTrac Software project.
 *
 * startapp:
 *
 * This class provides the necessary functionality to start the application,
 * including loading configuration files, initializing subsystems, and
 * launching the main application loop.
 *
 */

#ifndef STARTAPP_HPP
#define STARTAPP_HPP

#include <string>
#include <vector>
#include "Common/Utils/Json/structdef/AppConfig.h"

namespace PiTrac
{
// Alias for convenience
using ProcessOptions = AppConfig::ProcessOptions;

class StartApp
{
  public:
    StartApp() = delete;
    ~StartApp() = delete;

    /**
     * Starts a new process with the given executable and arguments.
     *
     * @param[in] executable The path to the executable to run.
     * @param[in] args The arguments to pass to the executable.
     * @param[in] processOpts Process configuration options (prctl, affinity,
     * etc.)
     *
     * @return The process ID of the started process.
     */
    const static pid_t startProcess
    (
        const std::string &executable,
        const std::vector<std::string> &args,
        const ProcessOptions &processOpts = {}
    );
};
} // namespace PiTrac

#endif // STARTAPP_HPP
