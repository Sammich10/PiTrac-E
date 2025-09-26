#include "Application/Agents/CameraAgent/CameraAgent.h"
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
PiTrac::CameraAgent *g_camera_task = nullptr;

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
    // Verify the minimum correct number of arguments
    if (argc < 2)
    {
        printf("Usage: CameraTaskLauncher <camera_index> <frame_buffer_size>\n");
        return EXIT_FAILURE;
    }
    // Parse argument 1 (camera index)
    if(!std::all_of(argv[1], argv[1] + std::strlen(argv[1]), ::isdigit))
    {
        printf("Invalid camera index argument, must be a non-negative integer\n");
        return EXIT_FAILURE;
    }
    else
    {
        size_t camera_index = std::stoul(argv[1]);
    }
    // Parse optional argument 2 (CPU core index)
    if(argc > 2 && !std::all_of(argv[2], argv[2] + std::strlen(argv[2]), ::isdigit))
    {
        printf("Invalid CPU core index argument, must be a non-negative integer less than %ld\n", PI_CPU_MAX);
        return EXIT_FAILURE;
    }
    else
    {
        size_t cpu_id = std::stoul(argv[2]);
        if(cpu_id > PI_CPU_MAX || cpu_id < 0)
        {
            printf("Invalid CPU core index, must be between 0 and %ld\n", PI_CPU_MAX);
            return EXIT_FAILURE;
        }
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu_id, &mask); // Set to run on specified CPU
        if(sched_setaffinity(0, sizeof(mask), &mask) != 0)
        {
            printf("Failed to set CPU affinity for Camera Agent Task\n");
            return EXIT_FAILURE;
        }
        printf("Set Camera Agent Task to run on CPU %zu\n", cpu_id);
    }

    size_t camera_index = std::stoul(argv[1]);
    try {
        printf("Starting Camera Agent Task Launcher\n");
        std::string procname = "CameraAgent_" + std::to_string(camera_index);
        prctl(PR_SET_NAME, procname.c_str(), 0, 0, 0);
        // Create and start the camera agent task
        PiTrac::CameraAgent camera_agent(camera_index);
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