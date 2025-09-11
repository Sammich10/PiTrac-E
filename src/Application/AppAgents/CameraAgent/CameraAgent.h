#ifndef CAMERA_AGENT_H
#define CAMERA_AGENT_H

#include "Application/AppAgents/AgentBase/GSAgentBase.h"
#include "Infrastructure/DataStructures/FrameBuffer.h"
#include "Interfaces/Camera/GSCameraInterface.h"
#include "Common/System/SystemModes.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <memory>

namespace PiTrac
{
/**
 * @brief The CameraAgent class is responsible for operating a camera
 * through the Camera Interface and publishing raw camera frames to a
 * messaging system.
 *
 * At it's core, it will implement different camera operations such as
 * opening, initializing, capturing frames, and closing the camera.
 * The CameraAgent will also enqueue captured frames into a FrameBuffer
 *
 * On top of this, it will support different modes of operation to support
 * the functionality of the launch monitor system.
 */
class CameraAgent : public GSAgentBase
{
  public:
    CameraAgent
    (
        std::unique_ptr<GSCameraInterface> camera_,
        std::shared_ptr<FrameBuffer> frame_buffer_,
        const uint32_t camera_id
    );

    ~CameraAgent();

    /**
     * @brief Sets up the camera agent.
     *
     * This method initializes the camera agent and prepares it for operation.
     *
     * @return true if setup was successful, false otherwise.
     */
    bool setup() override;

    /**
     * @brief Initializes the camera agent.
     *
     * This method sets up the necessary resources and configurations required
     * for the camera agent to operate.
     * It opens the camera, performs initial configuration, and prepares the
     * messaging system for frame publishing.
     *
     * @return true if initialization was successful, false otherwise.
     */
    bool initialize() override;

    /**
     * @brief Executes the main logic for the CameraAgent.
     *
     * This method starts the main capture loop for the camera agent, which
     * continuously captures frames from the camera and publishes them to the
     * messaging system.
     */
    void execute() override;

    /**
     * @brief Cleans up resources used by the camera agent.
     *
     * This method releases any camera resources, and joins the capture thread.
     */
    void cleanup() override;

  private:
    /**
     * @brief Continuously captures frames from the camera in a loop and
     * publishes them to the messaging system.
     */
    void captureLoop();

    std::shared_ptr<FrameBuffer> frame_buffer_;
    std::unique_ptr<GSCameraInterface> camera_;
    uint32_t camera_id_;
    std::atomic<bool> running_;
    uint64_t frame_counter_;
    SystemMode current_mode_;
};
} // namespace PiTrac

#endif // CAMERA_AGENT_H