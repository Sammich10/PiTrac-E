#include "Startup/startapp.h"
#include <iostream>
#include <sys/prctl.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
#include <cstring>

namespace PiTrac
{

const pid_t StartApp::startProcess(const std::string& executable, const std::vector<std::string>& args, const ProcessOptions& processOpts)
{
    // Convert std::string executable to const char*
    const char* exec_path = executable.c_str();
    
    // Convert std::vector<std::string> to char* array for execv
    std::vector<char*> argv;
    argv.reserve(args.size() + 2); // +1 for executable name, +1 for NULL terminator
    
    // First argument should be the executable name (convention for execv)
    argv.push_back(const_cast<char*>(executable.c_str()));
    
    // Add all arguments
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    // NULL terminate the array (required by execv)
    argv.push_back(nullptr);
    
    pid_t pid = fork();

    switch(pid)
    {
        case -1:
            perror("Failed to create forked process");
            break;
        case 0:
        {
            // Child process - configure before exec
            
            // Set process name (PR_SET_NAME)
            if (processOpts.processName.has_value()) {
                if (prctl(PR_SET_NAME, processOpts.processName.value().c_str()) == -1) {
                    perror("Failed to set process name");
                }
            }
            
            // Set CPU affinity
            if (processOpts.cpuAffinity.has_value()) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);
                for (int cpu : processOpts.cpuAffinity.value()) {
                    if (cpu >= 0 && cpu < CPU_SETSIZE) {
                        CPU_SET(cpu, &cpuset);
                    }
                }
                if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
                    perror("Failed to set CPU affinity");
                }
            }
            
            // Set scheduling policy and priority
            if (processOpts.schedPolicy.has_value()) {
                int policy = SCHED_OTHER; // default
                const std::string& policyStr = processOpts.schedPolicy.value();
                
                if (policyStr == "SCHED_FIFO") policy = SCHED_FIFO;
                else if (policyStr == "SCHED_RR") policy = SCHED_RR;
                else if (policyStr == "SCHED_OTHER") policy = SCHED_OTHER;
                
                struct sched_param param = {0};
                if (processOpts.schedPriority.has_value()) {
                    param.sched_priority = processOpts.schedPriority.value();
                }
                
                if (sched_setscheduler(0, policy, &param) == -1) {
                    perror("Failed to set scheduling policy");
                }
            }
            
            // Set process priority (nice value)
            if (processOpts.priority.has_value()) {
                if (setpriority(PRIO_PROCESS, 0, processOpts.priority.value()) == -1) {
                    perror("Failed to set process priority");
                }
            }
            
            // Set dumpable flag
            if (processOpts.dumpable.has_value()) {
                if (prctl(PR_SET_DUMPABLE, processOpts.dumpable.value() ? 1 : 0) == -1) {
                    perror("Failed to set dumpable flag");
                }
            }
            
            // Set keep capabilities
            if (processOpts.keepCaps.has_value()) {
                if (prctl(PR_SET_KEEPCAPS, processOpts.keepCaps.value() ? 1 : 0) == -1) {
                    perror("Failed to set keep capabilities");
                }
            }
            
            // Set death signal
            if (processOpts.deathSignal.has_value()) {
                if (prctl(PR_SET_PDEATHSIG, processOpts.deathSignal.value()) == -1) {
                    perror("Failed to set parent death signal");
                }
            }
            
            // Change working directory
            if (processOpts.workingDirectory.has_value()) {
                if (chdir(processOpts.workingDirectory.value().c_str()) == -1) {
                    perror("Failed to change working directory");
                }
            }
            
            // Set environment variables
            if (processOpts.environment.has_value()) {
                for (const auto& [key, value] : processOpts.environment.value()) {
                    if (setenv(key.c_str(), value.c_str(), 1) == -1) {
                        perror(("Failed to set environment variable: " + key).c_str());
                    }
                }
            }
            
            // Execute the program
            execvp(exec_path, argv.data());
            perror("Failed to execute process");
            exit(EXIT_FAILURE);
        }
        default:
        {
            // Parent process
            break;
        }
    }

    return pid;
}

} // namespace PiTrac