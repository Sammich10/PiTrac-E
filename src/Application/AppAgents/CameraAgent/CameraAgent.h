#ifndef CAMERA_AGENT_H
#define CAMERA_AGENT_H

#include "Infrastructure/Messaging/Messagers/GSMessagerBase.h"
#include "Infrastructure/Messaging/Messages/GSCameraFrameRawMsg.h"
#include "Interfaces/Camera/GSCameraInterface.h"
#include "Infrastructure/TaskProcess/GSTask.h"
#include "Common/System/SystemModes.h"
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>

namespace PiTrac
{
class CameraAgent : public GSTask
{
    /**
     * @brief The CameraAgent class is responsible for operating a camera
     *through the Camera Interface and publishing raw camera frames to a
     *messaging system.
     *
     * At it's core, it will implement different camera operations such as
     *opening, initializing, capturing frames, and closing the camera.
     * The CameraAgent will also handle the messaging of captured frames to
     *other components in the system (when in the appropriate mode).
     *
     * On top of this, it will support different modes of operation to support
     *the functionality of the launch monitor system, be able to notify other
     *tasks
     * when events occur, and manage the lifecycle of the camera interface.
     */

  public:
    CameraAgent
    (
        std::unique_ptr<GSCameraInterface> camera_,
        const std::string &camera_id,
        const std::string &endpoint
    );

    ~CameraAgent();

    bool initialize() override;

    void execute() override;

    void cleanup() override;

  private:
    void captureLoop();

    std::unique_ptr<GSMessagerBase> publisher_;
    std::unique_ptr<GSMessagerBase> subscriber_;
    std::unique_ptr<GSCameraInterface> camera_;
    std::string camera_id_;
    std::atomic<bool> running_;
    std::thread capture_thread_;
    uint64_t frame_counter_;
    std::string endpoint_;
    SystemMode current_mode_;
};
} // namespace PiTrac

#endif // CAMERA_AGENT_H