#include "Application/Agents/FlightProcessor/FlightProcessor.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>
#include <atomic>
#include <sys/prctl.h>
#include <sched.h>

constexpr size_t PI_CPU_MAX = 3;

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);
PiTrac::FlightProcessor *g_camera_task = nullptr;

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        printf("Shutdown signal received, requesting graceful shutdown...\n\n");
        g_shutdown_requested.store(true);
        if (g_camera_task)
        {
            g_camera_task->end();
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    // No arguments expected for the FlightProcessor, but we can add argument parsing here if needed in the future (e.g., for debug mode, config file path, etc)
    try {
        printf("Starting Camera Agent Task Launcher\n");
        std::string procname = "FlightProcessor";
        prctl(PR_SET_NAME, procname.c_str(), 0, 0, 0);
        // Create and start the camera agent task
        PiTrac::FlightProcessor camera_agent(procname);
        // Set CPU affinity to limit the camera agent to specific cores (e.g., 0-3)
        g_camera_task = &camera_agent;

        if (!camera_agent.run())
        {
            printf("Failed to start camera agent task\n");
            return EXIT_FAILURE;
        }

        printf("Camera Agent Task started successfully\n");
    } catch (const std::exception &e) {
        printf("Exception occurred in Camera Agent Task Launcher: %s\n", e.what());
        return EXIT_FAILURE;
    }
    printf("Camera Agent Task Launcher exiting normally\n");
    return EXIT_SUCCESS;
}