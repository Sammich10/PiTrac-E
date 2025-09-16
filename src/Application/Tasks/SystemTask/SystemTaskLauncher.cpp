#include "Application/Tasks/SystemTask/SystemTask.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>
#include <atomic>


// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);
PiTrac::SystemTask *g_system_task = nullptr;
std::shared_ptr<PiTrac::GSLogger> logger = PiTrac::GSLogger::getInstance();

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        logger->info("Shutdown signal received, requesting graceful shutdown...");
        g_shutdown_requested.store(true);
        if (g_system_task)
        {
            g_system_task->stop();
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        logger->info("Starting System Task Launcher");

        // Create and start the system task
        PiTrac::SystemTask system_task;
        g_system_task = &system_task;
        if (!system_task.start())
        {
            logger->error("Failed to start system task");
            return EXIT_FAILURE;
        }

        logger->info("System Task started successfully");
    } catch (const std::exception &e) {
        logger->error("Exception occurred in System Task Launcher: " + std::string(e.what()));
        return EXIT_FAILURE;
    }
    logger->info("System Task Launcher exiting normally");
    return EXIT_SUCCESS;
}