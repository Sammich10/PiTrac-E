#ifndef APPCONFIG_STRUCT_H
#define APPCONFIG_STRUCT_H

#include <string>
#include <vector>
#include <map>
#include <optional>

namespace PiTrac
{

struct AppConfig
{
    struct ProcessOptions
    {
        // Process naming
        std::optional<std::string> processName;          // PR_SET_NAME
        
        // CPU affinity
        std::optional<std::vector<int>> cpuAffinity;     // sched_setaffinity
        
        // Scheduling
        std::optional<int> priority;                     // setpriority/nice
        std::optional<std::string> schedPolicy;          // SCHED_FIFO, SCHED_RR, SCHED_OTHER
        std::optional<int> schedPriority;                // sched_setparam
        
        // Memory
        std::optional<bool> dumpable = true;             // PR_SET_DUMPABLE
        std::optional<bool> keepCaps = false;            // PR_SET_KEEPCAPS
        
        // Death signal
        std::optional<int> deathSignal;                  // PR_SET_PDEATHSIG
        
        // Working directory
        std::optional<std::string> workingDirectory;
        
        // Environment variables to set
        std::optional<std::map<std::string, std::string>> environment;
        
        ProcessOptions() = default;
    };
    
    struct ExecConfig
    {
        bool Enable = false;
        std::string Executable;
        std::vector<std::string> Arguments;
        ProcessOptions processOpts;
        
        ExecConfig() = default;
    };

    std::map<std::string, ExecConfig> Executables;
};

} // namespace PiTrac

#endif // APPCONFIG_STRUCT_H