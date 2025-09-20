#include "Application/Managers/SystemManager/SystemManager.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>
#include <atomic>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);
PiTrac::SystemManager *g_system_manager_task = nullptr;
std::shared_ptr<PiTrac::GSLogger> logger = PiTrac::GSLogger::getInstance();

void signalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM)
    {
        logger->info("Shutdown signal received, requesting graceful shutdown...");
        g_shutdown_requested.store(true);
        if (g_system_manager_task)
        {
            g_system_manager_task->end();
        }
    }
}

int main(int argc, char *argv[])
{
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    try {
        logger->info("Starting System Manager");

        // Create and start the system manager
        PiTrac::SystemManager system_manager;
        g_system_manager_task = &system_manager;

        if (!system_manager.run())
        {
            logger->error("Failed to start system manager");
            return EXIT_FAILURE;
        }

        logger->info("System Manager started successfully");
    } catch (const std::exception &e) {
        logger->error("Exception occurred in System Manager Launcher: " + std::string(e.what()));
        return EXIT_FAILURE;
    }
    logger->info("System Manager Launcher exiting normally");
    return EXIT_SUCCESS;
}