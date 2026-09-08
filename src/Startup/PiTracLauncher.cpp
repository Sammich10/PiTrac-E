#include "Startup/startapp.h"
#include "Common/Utils/Json/jsonparser.h"
#include "Common/Utils/FileUtils/FileUtils.h"

/**
 * @brief Starts all PiTrac application processes defined in the AppConfig.
 *
 * @param config The AppConfig containing executables to start.
 * @return int Exit code (0 for success).
 */
int startPiTrac(const PiTrac::AppConfig &config)
{
    std::cout << "Starting PiTrac application..." << std::endl;
    for(const auto &[name, exec] : config.Executables)
    {
        if(exec.Enable)
        {
            if(!PiTrac::FileUtils::executableExists(exec.Executable))
            {
                std::cerr << "Executable not found or not executable: " << name << " (" << exec.Executable << ")" << std::endl;
                continue;
            }
            const pid_t pid = PiTrac::StartApp::startProcess(exec.Executable, exec.Arguments, exec.processOpts);
            if(pid < 0)
            {
                std::cerr << "Failed to start process: " << name << " (" << exec.Executable << ")" << std::endl;
            }
            else
            {
                std::cout << "Started process: " << name << " (PID: " << pid << ", Executable: " << exec.Executable << ")" << std::endl;
            }
        }
        else
        {
            std::cout << "Skipping disabled executable: " << name << std::endl;
        }
    }
    return 0;
}

/**
 * @brief Stops all PiTrac application processes defined in the AppConfig. Sends
 * SIGINT to each process to request graceful shutdown.
 *
 * @param config The AppConfig containing executables to stop.
 * @return int Exit code (0 for success).
 */
int stopPiTrac(const PiTrac::AppConfig &config)
{
    std::cout << "Stopping PiTrac application..." << std::endl;
    for(const auto &[name, exec] : config.Executables)
    {
        if(exec.Enable)
        {
            // Send SIGINT to all processes with this executable name
            std::string cmd = "pkill -SIGINT -f \"" + exec.Executable + "*\"";
            int ret = system(cmd.c_str());
            if(ret != 0)
            {
                std::cerr << "Failed to stop process: " << name << " (" << exec.Executable << ")" << std::endl;
            }
            else
            {
                std::cout << "Stopped process: " << name << " (" << exec.Executable << ")" << std::endl;
            }
        }
    }
    return 0;
}

/**
 * @brief Kills all PiTrac application processes defined in the AppConfig. Sends
 * SIGKILL to each process.
 *
 * @param config The AppConfig containing executables to kill.
 * @return int Exit code (0 for success).
 */
int killPiTrac(const PiTrac::AppConfig &config)
{
    std::cout << "Killing PiTrac application..." << std::endl;
    for(const auto &[name, exec] : config.Executables)
    {
        if(exec.Enable)
        {
            // Send SIGKILL to all processes with this executable name
            std::string cmd = "pkill -SIGKILL -f \"" + exec.Executable + "*\"";
            int ret = system(cmd.c_str());
            if(ret != 0)
            {
                std::cerr << "Failed to kill process: " << name << " (" << exec.Executable << ")" << std::endl;
            }
            else
            {
                std::cout << "Killed process: " << name << " (" << exec.Executable << ")" << std::endl;
            }
        }
    }
    return 0;
}

/**
 * @brief Main entry point for PiTrac launcher. Parses command line arguments
 * for the command and app config path.
 *
 * Usage: PiTrac <command> [app_config_path]
 *       command: start | stop | kill
 *       app_config_path: Path to the application configuration JSON file. If
 * not provided, uses $PITRAC_APP_CONFIG environment variable.
 */
int main(int argc, char *argv[])
{
    if(argc < 2)
    { // Must provide command
        std::cerr << "Usage: PiTrac <command> [app_config_path]" << std::endl;
        return EXIT_FAILURE;
    }
    std::string app_config_path = "";
    if(argc < 3)
    { // Config path not explicitly provided, attempt to use default from
      // environment variable
        const char *env_path = std::getenv("PITRAC_APP_CONFIG");
        if(env_path == nullptr)
        {
            std::cerr << "App config path not provided and Pitrac environment variable not set." << std::endl;
            return EXIT_FAILURE;
        }
        else
        {
            app_config_path = std::string(env_path);
        }
    }
    else
    {
        app_config_path = std::string(argv[2]);
    }
    const std::string command = argv[1];
    if(!PiTrac::FileUtils::fileExists(app_config_path))
    {
        std::cerr << "App config file does not exist: " << app_config_path << std::endl;
        return EXIT_FAILURE;
    }

    struct PiTrac::AppConfig app_config;
    try{
        app_config = PiTrac::JsonParser::parseAppConfig(app_config_path, "Executables");
    }
    catch(const std::exception &e)
    {
        std::cerr << "Error parsing AppConfig: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    if(command == "start")
    {
        return startPiTrac(app_config);
    }
    else if(command == "stop")
    {
        return stopPiTrac(app_config);
    }
    else if(command == "kill")
    {
        return killPiTrac(app_config);
    }
    else
    {
        std::cerr << "Unknown command: " << command << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}