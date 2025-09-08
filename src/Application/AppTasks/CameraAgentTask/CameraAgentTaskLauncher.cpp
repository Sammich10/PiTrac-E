#include "Application/AppTasks/CameraAgentTask/CameraAgentTask.h"
#include "Common/Utils/Logging/GSLogger.h"
#include <cstdlib>
#include <thread>
#include <chrono>
#include <iostream>
#include <signal.h>
#include <atomic>

// Global flag for graceful shutdown
std::atomic<bool> g_shutdown_requested(false);
PiTrac::CameraAgentTask* g_camera_task = nullptr;
std::shared_ptr<PiTrac::GSLogger> logger = PiTrac::GSLogger::getInstance();

// Signal handler for graceful shutdown
void signalHandler(int signal) {
    // Add immediate output that bypasses logger buffering
    fprintf(stderr, "\n*** SIGNAL %d RECEIVED ***\n", signal);
    fflush(stderr);
    
    logger->error("Signal received: " + std::to_string(signal));
    g_shutdown_requested = true;
    
    if (g_camera_task) {
        fprintf(stderr, "Calling stop() on camera task...\n");
        fflush(stderr);
        g_camera_task->stop();
    } else {
        fprintf(stderr, "ERROR: No camera task reference!\n");
        fflush(stderr);
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);   // Ctrl+C
    signal(SIGTERM, signalHandler);  // Termination request
    signal(SIGUSR1, signalHandler);  // User signal for graceful shutdown
    
    try {
        logger->info("Starting Camera Agent Task Launcher");
        
        // Create and start the camera agent task
        PiTrac::CameraAgentTask camera_agent_task;
        g_camera_task = &camera_agent_task;

        if (!camera_agent_task.start()) {
            logger->error("Failed to start camera agent task");
            return EXIT_FAILURE;
        }

        logger->info("Camera Agent Task started successfully (PID: " + std::to_string(camera_agent_task.getProcessId()) + ")");
        logger->info("Press Ctrl+C to stop...");
        
        // Keep the main thread alive while the agent task is running
        while (!g_shutdown_requested.load() && camera_agent_task.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        logger->info("Shutdown requested, stopping Camera Agent Task...");
        
        // Stop the task gracefully
        camera_agent_task.stop();
        
        // Wait for task to exit
        if (!camera_agent_task.waitForExit(10)) {
            logger->error("Task did not exit gracefully, force killing...");
            camera_agent_task.forceKill();
        }
        
        logger->info("Camera Agent Task stopped");
        
    } catch (const std::exception& e) {
        logger->error("Exception occurred in Camera Agent Task Launcher: " + std::string(e.what()));
        return EXIT_FAILURE;
    } catch (...) {
        logger->error("Unknown exception occurred");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}