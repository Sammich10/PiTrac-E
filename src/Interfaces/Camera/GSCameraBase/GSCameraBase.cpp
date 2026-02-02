#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
#include "Common/Utils/CameraUtils/CameraUtils.h"
#include <iostream>
#include <fstream>
#include <thread>

namespace PiTrac
{
GSCameraBase::~GSCameraBase()
{
    closeCamera();
}

bool GSCameraBase::openCamera()
{
    if(isCameraOpen_)
    {
        logger_->error("Camera already open");
        return true;
    }
    logger_->info("Opening camera at index " + std::to_string(cameraIndex_));
    try
    {
        // Get available cameras
        auto cameras = cameraManager_->cameras();
        if (cameras.empty())
        {
            logger_->error("No cameras found");
            return false;
        }
        // Ensure the camera index is valid
        if (cameraIndex_ >= cameras.size())
        {
            logger_->error("Camera index " + std::to_string(cameraIndex_) + " out of range");
            return false;
        }
        // Select the camera, log the camera ID for reference
        camera_ = cameras[cameraIndex_];
        std::string cameraId = camera_->id();
        logger_->info("Using camera: " + cameraId);
        // Acquire the camera
        const int ret = camera_->acquire();
        if (ret)
        {
            logger_->error("Failed to acquire camera: " + cameraId);
            return false;
        }
        // Create new allocator (will be used during stream configuration)
        allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
        if(!allocator_)
        {
            logger_->error("Failed to create FrameBufferAllocator");
            return false;
        }
        destroyRequests();
        isCameraOpen_ = true;
        return isCameraOpen_;
    }
    catch (const std::exception &e)
    {
        logger_->error("Exception in openCamera: " + std::string(e.what()));
        return false;
    }
}

void GSCameraBase::closeCamera()
{
    if (camera_)
    {
        stopContinuousCapture();
        if (isCameraOpen_)
        {
            camera_->stop();
            cameraStarted_ = false;
        }
        camera_->requestCompleted.disconnect(this, &GSCameraBase::requestComplete);
        // Clean up requests
        destroyRequests();
        // Free all allocated buffers
        if (allocator_)
        {
            for (size_t i = 0; i < config_->size(); ++i)
            {
                libcamera::Stream *stream = config_->at(i).stream();
                const std::vector<std::unique_ptr<libcamera::FrameBuffer> > &buffers =
                    allocator_->buffers(stream);
                if (!buffers.empty())
                {
                    allocator_->free(stream);
                }
            }
        }
        if(hasFramesAvailable())
        {
            clearFrameBuffer();
        }
        allocator_.reset();
        camera_->release();
        camera_.reset();
    }
    isCameraOpen_ = false;
    isConfigured_ = false;
    // Attempt to give IPA processes time to cleanup after libcamera shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

bool GSCameraBase::start()
{
    // Check if already started
    if(cameraStarted_)
    {
        logger_->info("Camera already started");
        return true;
    }
    // Ensure the camera has been configured
    if(!isConfigured_)
    {
        logger_->error("Camera not configured, cannot start");
        return false;
    }
    // Create buffers for the stream
    if(!allocateBuffers())
    {
        logger_->error("Failed to allocate buffers before starting camera");
        return false;
    }
    // Start the camera
    const int ret = camera_->start();
    if (ret)
    {
        logger_->error("Failed to start camera: " + std::to_string(ret));
        return false;
    }
    // At this point, the camera is running and requests can be created and
    // queued, but we leave that to the caller
    cameraStarted_ = true;
    return true;
}

bool GSCameraBase::stop()
{
    // Step 1: Stop the camera if it is started
    if (cameraStarted_)
    {
        int ret = camera_->stop();
        if (ret)
        {
            logger_->error("Failed to stop camera: " + std::to_string(ret));
            return false;
        }
        cameraStarted_ = false;
        logger_->info("Camera stopped successfully");
        // Ensure all pending requests are completed
        for(size_t i = 0; i < requests_.size(); ++i)
        {
            libcamera::Request *request = requests_[i].get();
            if (request->status() == libcamera::Request::RequestPending)
            {
                logger_->info("Waiting for request " + std::to_string(i) + " to complete...");
                const int timeout_ms = 5000; // 5 seconds timeout
                auto start_time = std::chrono::steady_clock::now();
                // Simple wait loop (could be improved with condition variable)
                while (request->status() == libcamera::Request::RequestPending)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    auto elapsed = std::chrono::steady_clock::now() - start_time;
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() > timeout_ms)
                    {   // Timeout reached
                        logger_->error("Timeout waiting for request " + std::to_string(i) + " to complete");
                        break;
                    }
                }
            }
        }
        // Clear all requests after stopping the camera
        destroyRequests();
        // Free all allocated buffers after stopping the camera
        if(!freeBuffers())
        {
            logger_->error("Failed to free buffers after stopping camera");
            return false;
        }
        // The camera is now stopped and all resources have been cleaned up.
        // It is safe to reconfigure or close the camera.
    }
    return true;
}

cv::Mat GSCameraBase::captureFrame(uint32_t timeout_ms)
{
    if (!isCameraOpen_ || !isConfigured_)
    {   // Camera not ready for capture, log error and return empty frame
        logger_->error("Camera not open or configured");
        return cv::Mat();
    }
    else if(isCapturing_)
    {   // Attempt to return the latest frame captured in continuous mode, if
        // available.
        // This will not work if continuous capture was started with a frame
        // callback to handle frames.
        return getLatestFrame();
    }
}

bool GSCameraBase::setTriggerMode(TriggerMode mode)
{
    if (isCapturing_)
    {
        logger_->error("Cannot change trigger mode while capturing");
        return false;
    }

    // Reconfigure camera if already configured
    if (isConfigured_ && configureTriggerMode(mode))
    {
        triggerMode_ = mode;
        return true;
    }

    return false;
}

bool GSCameraBase::startContinuousCapture(requestCompleteCallback callback)
{
    logger_->info("Starting continuous capture...");
    if (!isCameraOpen_ || !isConfigured_)
    {
        logger_->error("Camera not open or configured");
        return false;
    }
    // Check if already capturing
    if (isCapturing_)
    {
        logger_->info("Camera already capturing");
        return true;
    }
    // Create requests for the allocated buffers if none exist
    if(requests_.empty())
    {
        if(!createRequests())
        {
            logger_->error("Failed to create requests before starting camera");
            return false;
        }
        logger_->info("Created " + std::to_string(requests_.size()) + " requests for continuous capture");
    }
    // Start the camera and begin continuous capture
    try {
        // Start camera if not running
        if (!cameraStarted_)
        {
            logger_->error("Failed to start continuous capture, camera not started");
            return false;
        }
        requestCallback_ = callback;
        // Connect the request completed signal to the handler
        camera_->requestCompleted.connect(this, &GSCameraBase::requestComplete);
        // Queue initial requests
        logger_->info("Queueing " + std::to_string(requests_.size()) + " initial requests for continuous capture...");
        for (auto &request : requests_)
        {
            camera_->queueRequest(request.get());
        }
        isCapturing_ = true;
        return true;
    } catch (const std::exception &e) {
        logger_->error("Exception in startContinuousCapture: " + std::string(e.what()));
        return false;
    }
}

bool GSCameraBase::stopContinuousCapture()
{
    if (!isCapturing_)
    {
        return true; // Already stopped
    }
    // Setting isCapturing_ to false will signal the requestComplete handler to
    // stop processing frames
    // so all queued requests will be processed but no new requests will be
    // queued.
    logger_->info("Stopping continuous capture...");
    isCapturing_ = false;
    return true;
}

bool GSCameraBase::configureStream(const libcamera::StreamRole &streamRole)
{
    if(!stop())
    {   // Ensure the camera is stopped. If the camera was not stopped
        // successfully, log error and return false
        // This prevents reconfiguration while the camera is active. If the
        // camera was already stopped, this is a no-op.
        logger_->error("Failed to stop camera for reconfiguration");
        return false;
    }

    // Create new single-stream configuration for the active stream
    if (!reconfigureForActiveStream(streamRole))
    {
        logger_->error("Failed to reconfigure for active stream");
        return false;
    }
    logger_->info("Successfully configured stream");
    return true;
}

bool GSCameraBase::allocateBuffers()
{
    if (!allocator_)
    {
        logger_->error("Cannot allocate buffers, no buffer allocator");
        return false;
    }
    if(!isConfigured_)
    {
        logger_->error("Cannot allocate buffers, camera not configured");
        return false;
    }
    if(isCapturing_)
    {
        logger_->error("Cannot allocate buffers while capturing");
        return false;
    }

    // Allocate buffers for all streams in the current configuration (there
    // should only be 1 active stream, but handle all for safety)
    for (size_t i = 0; i < config_->size(); ++i)
    {
        if (!allocateBuffersForStream(config_->at(i).stream()))
        {
            logger_->error("Failed to allocate buffers for active stream");
            return false;
        }
    }

    return true;
}

bool GSCameraBase::freeBuffers()
{
    // Free all allocated buffers
    if(!allocator_)
    {
        logger_->error("Allocator not initialized");
        return false;
    }
    for (size_t i = 0; i < config_->size(); ++i)
    {
        logger_->info("Freeing buffers for stream " + std::to_string(i));
        libcamera::Stream *stream = config_->at(i).stream();
        const std::vector<std::unique_ptr<libcamera::FrameBuffer> > &buffers =
            allocator_->buffers(stream);
        if (!buffers.empty())
        {   // Free buffers for this stream
            allocator_->free(stream);
        }
    }
    // Buffers freed successfully
    return true;
}

bool GSCameraBase::createRequests()
{
    for(size_t i = 0; i < config_->size(); ++i)
    {
        libcamera::Stream *stream = config_->at(i).stream();
        const std::vector<std::unique_ptr<libcamera::FrameBuffer> > &buffers = allocator_->buffers(stream);
        for (size_t j = 0; j < buffers.size(); ++j)
        {
            std::unique_ptr<libcamera::Request> request = camera_->createRequest();
            if (!request)
            {
                logger_->error("Failed to create request " + std::to_string(j) + " for stream " + std::to_string(i));
                return false;
            }
            // Set controls (exposure, frame duration, gain, etc.)
            libcamera::ControlList &controls = request->controls();
            // controls.set(libcamera::controls::ExposureTimeMode,
            // libcamera::controls::ExposureTimeMode::Manual);
            controls.set(libcamera::controls::ExposureTime, currentExposureUs_);
            controls.set(libcamera::controls::FrameDurationLimits,
                         {static_cast<int64_t>(1000000.0f / currentFps_),
                          static_cast<int64_t>(1000000.0f / currentFps_)});
            // controls.set(libcamera::controls::AnalogueGainMode,
            // libcamera::controls::AnalogueGainMode::Manual);
            controls.set(libcamera::controls::AnalogueGain, analogGain_);
            // controls.set(libcamera::controls::DigitalGain, digitalGain_);
            // Add buffer to request
            const int ret = request->addBuffer(stream, buffers[j].get());
            if (ret < 0)
            {
                logger_->error("Failed to add buffer to request");
                return false;
            }
            requests_.push_back(std::move(request));
        }
        logger_->info("Created " + std::to_string(buffers.size()) + " requests for stream " + std::to_string(i));
    }
    return true;
}

bool GSCameraBase::destroyRequests()
{
    requests_.clear();
    return true;
}

bool GSCameraBase::hasFramesAvailable() const
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);
    return !frameBuffer_.empty();
}

size_t GSCameraBase::getFrameQueueSize() const
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);
    return frameBuffer_.size();
}

cv::Mat GSCameraBase::getLatestFrame()
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);
    if (frameBuffer_.empty())
    {
        logger_->warning("No frames available in buffer");
        return cv::Mat(); // Return empty Mat if no frames are available
    }
    cv::Mat latestFrame = frameBuffer_.back().clone(); // Get the latest frame
    // Clear the buffer after retrieving the latest frame
    while (!frameBuffer_.empty())
    {
        frameBuffer_.pop();
    }
    return latestFrame;
}

void GSCameraBase::clearFrameBuffer()
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);
    while (!frameBuffer_.empty())
    {
        frameBuffer_.pop();
    }
}

std::string GSCameraBase::toString() const
{
    return "GSCameraBase [" + std::to_string(resolutionX_) + "x" + std::to_string(resolutionY_) +
           ", FL:" + std::to_string(focalLength_mm_) + "mm]";
}

bool GSCameraBase::allocateBuffersForStream(libcamera::Stream *stream)
{
    int ret = allocator_->allocate(stream);
    if (ret < 0)
    {
        logger_->error("Failed to allocate buffers for stream");
        return false;
    }
    size_t allocated = allocator_->buffers(stream).size();
    if(allocated == 0)
    {
        logger_->error("No buffers allocated for stream");
        return false;
    }
    else if(allocated < numBuffers_)
    {
        logger_->warning("Allocated fewer buffers (" + std::to_string(allocated) +
                         ") than requested (" + std::to_string(numBuffers_) + ")");
    }
    else
    {
        logger_->info("Successfully allocated " + std::to_string(allocated) +
                      " buffers for stream");
    }
    return true;
}

bool GSCameraBase::configureTriggerMode(const TriggerMode &mode)
{
    if (!camera_ || !config_)
    {
        return false;
    }

    // Set trigger mode controls
    libcamera::ControlList controls;

    if (mode == TriggerMode::EXTERNAL_TRIGGER)
    {
        logger_->error("GSCameraBase does not support external trigger mode");
        return false;
    }
    else
    {
        // Configure for free running
        int64_t frameDuration = static_cast<int64_t>(1000000.0f / currentFps_); // microseconds
        controls.set(libcamera::controls::FrameDurationLimits, {frameDuration, frameDuration});
    }

    // Apply controls to all requests
    for (auto &request : requests_)
    {
        request->controls().merge(controls);
    }

    return true;
}

void GSCameraBase::addFrameToBuffer(const cv::Mat &frame)
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);

    // Add frame to buffer
    frameBuffer_.push(frame.clone());

    // Enforce maximum buffer size
    while (frameBuffer_.size() > maxFrameBuffer_)
    {
        frameBuffer_.pop(); // Remove oldest frame
    }
}

void GSCameraBase::requestComplete(libcamera::Request *request)
{
    libcamera::Request::Status status = request->status();
    switch(status)
    {
        case libcamera::Request::RequestComplete:
        {
            // Since we only have one stream active at a time, it's always at
            // index 0
            libcamera::FrameBuffer *buffer = request->findBuffer(config_->at(0).stream());
            if (buffer)
            {   // Process the completed buffer
                cv::Mat frame = CameraUtils::convertBufferToMat(buffer, config_->at(0));
                if(frame.empty())
                {
                    logger_->error("Failed to convert buffer to cv::Mat in requestComplete");
                    return;
                }
                if(requestCallback_)
                {   // Invoke user-defined callback if set
                    requestCallback_(frame);
                }
                else
                {   // Default processing: convert buffer to cv::Mat and add to
                    // software frame buffer
                    addFrameToBuffer(frame);
                }
            }
            else
            {
                logger_->error("Failed to find buffer for completed request");
            }
            // If still capturing, reuse and re-queue the request
            if (isCapturing_)
            {
                request->reuse(libcamera::Request::ReuseBuffers);
                camera_->queueRequest(request);
            }
            break;
        }
        case libcamera::Request::RequestCancelled:
        {
            // During shutdown, we expect RequestCancelled errors (status 2)
            if (!isCapturing_)
            {
                // This is expected during shutdown - don't log as error
                logger_->warning("Request cancelled during shutdown (expected)");
            }
            else
            {
                logger_->error("Request cancelled unexpectedly");
            }
            break;
        }
        case libcamera::Request::RequestPending:
        {
            // This should not happen in requestComplete
            logger_->error("Request is still pending in requestComplete");
            break;
        }
        default:
        {
            logger_->error("Unknown request status in requestComplete: " + std::to_string(status));
            break;
        }
    }
}

bool GSCameraBase::reconfigureForActiveStream(const libcamera::StreamRole &streamRole)
{
    // Generate base configuration for stream
    config_ = camera_->generateConfiguration({streamRole});
    if (!config_)
    {
        logger_->error("Failed to generate configuration for stream");
        return false;
    }
    // Configure stream. We only have one active stream at a time in this
    // implementation.
    libcamera::StreamConfiguration &streamConfig = config_->at(0);
    streamConfig.size.width = resolutionX_;
    streamConfig.size.height = resolutionY_;
    streamConfig.pixelFormat = pixelFormat_;
    streamConfig.bufferCount = numBuffers_;
    config_->orientation = sensorOrientation_;
    // Validate configuration
    libcamera::CameraConfiguration::Status validation = config_->validate();
    switch(validation)
    {
        case libcamera::CameraConfiguration::Adjusted:
            logger_->warning("Stream configuration adjusted by validation");
            // Update configuration parameters. Some have changed after
            // validation.
            resolutionX_ = streamConfig.size.width;
            resolutionY_ = streamConfig.size.height;
            pixelFormat_ = streamConfig.pixelFormat;
            numBuffers_ = streamConfig.bufferCount;
        // FALLTHROUGH //
        case libcamera::CameraConfiguration::Valid:
            // Update derived parameters after validation
            stride_ = streamConfig.stride;
            frameSizeBytes_ = streamConfig.frameSize;
            break;
        case libcamera::CameraConfiguration::Invalid:
            logger_->error("Stream configuration invalid after validation");
            return false;
        default:
            break;
    }
    // Apply configuration
    const int ret = camera_->configure(config_.get());
    if (ret)
    {
        logger_->error("Failed to configure camera for stream");
        return false;
    }
    logger_->info("Configured stream: " + streamConfig.toString());
    isConfigured_ = true;
    return true;
}
} // namespace PiTrac