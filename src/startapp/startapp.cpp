#include "startapp.hpp"
#include <iostream>
#include <sys/prctl.h>
#include <errno.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <unistd.h>

int appStart(int argc, const char* argv[]) {
    return 0; // Placeholder for application start logic
}

const pid_t StartApp::startProcess(const char *executable, const char *const args[], const int opts) 
{

    pid_t pid = fork();

    switch(pid)
    {
        case -1:
            perror("Failed to create forked process");
            break;
        case 0: 
        {
            execv(executable, const_cast<char *const *>(args));
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

bool StartApp::fileExists(const std::string &path) {
    return access(path.c_str(), F_OK) != -1;
}