#include "Startup/startapp.h"
#include "Common/Utils/Json/jsonparser.h"
#include "Common/Utils/FileUtils/FileUtils.h"

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

int main(int argc, char *argv[])
{
    if(argc < 3)
    {
        std::cerr << "Usage: PiTrac <command> <app_config_path>" << std::endl;
        return EXIT_FAILURE;
    }
    const std::string command = argv[1];
    const std::string app_config_path = argv[2];
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