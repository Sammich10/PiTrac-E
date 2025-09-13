#include "Application/Tasks/CameraTask/CameraTask.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>
#include <atomic>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);
PiTrac::CameraTask *g_camera_task = nullptr;
std::shared_ptr<PiTrac::GSLogger> logger = PiTrac::GSLogger::getInstance();

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        logger->info("Shutdown signal received, requesting graceful shutdown...");
        g_shutdown_requested.store(true);
        if (g_camera_task)
        {
            g_camera_task->stop();
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    // Parse arguments. Arg 1 is camera index, arg 2 is frame buffer size
    if (argc < 3)
    {
        logger->error("Usage: CameraTaskLauncher <camera_index> <frame_buffer_size>");
        return EXIT_FAILURE;
    }
    size_t camera_index = std::stoul(argv[1]);
    size_t frame_buffer_size = std::stoul(argv[2]);
    try {
        logger->info("Starting Camera Agent Task Launcher");

        // Create and start the camera agent task
        PiTrac::CameraTask camera_agent_task(camera_index, frame_buffer_size);
        g_camera_task = &camera_agent_task;

        if (!camera_agent_task.start())
        {
            logger->error("Failed to start camera agent task");
            return EXIT_FAILURE;
        }

        logger->info("Camera Agent Task started successfully");
    } catch (const std::exception &e) {
        logger->error("Exception occurred in Camera Agent Task Launcher: " + std::string(e.what()));
        return EXIT_FAILURE;
    }
    logger->info("Camera Agent Task Launcher exiting normally");
    return EXIT_SUCCESS;
}