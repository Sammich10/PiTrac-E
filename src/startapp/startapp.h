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

class StartApp {
public:
    StartApp();
    ~StartApp();

    void loadConfig
    (
        const std::string& configFile
    );

    /**
     * Starts the application with the given arguments.
     * 
     * @param[in] argc The number of command line arguments.
     * @param[in] argv The command line arguments.
     * 
     * @return An integer indicating the exit status of the application.
     */
    int appStart
    (
        int argc,
        const char* argv[]
    );

    /**
     * Starts a new process with the given executable and arguments.
     * 
     * @param[in] executable The path to the executable to run.
     * @param[in] args The arguments to pass to the executable.
     * @param[in] opts Options for process creation (e.g., flags).
     * 
     * @return The process ID of the started process.
     */
    const static pid_t startProcess
    (
        const char *executable,
        const char *const args[],
        const int opts
    );

    /**
     * @brief Checks to see if a file exists at the given path.
     * 
     * @param[in] path The path to the file to check.
     * 
     * @return True if the file exists, false otherwise.
     */
    static bool fileExists
    (
        const std::string &path
    );

private:
    std::string executable;
    std::vector<std::string> arguments;
};

#endif // STARTAPP_HPP
