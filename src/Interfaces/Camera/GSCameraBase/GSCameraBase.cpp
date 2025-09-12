#include "Interfaces/Camera/GSCameraBase/GSCameraBase.h"
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
    try {
        // Get available cameras
        auto cameras = cameraManager_->cameras();
        if (cameras.empty())
        {
            logger_->error("No cameras found");
            return false;
        }

        // Find IMX296 camera (or use the specified index)
        if (cameraIndex_ >= cameras.size())
        {
            logger_->error("Camera index " + std::to_string(cameraIndex_) + " out of range");
            return false;
        }

        camera_ = cameras[cameraIndex_];

        // Verify this is an IMX296 (optional check)
        std::string cameraId = camera_->id();
        std::cout << "Using camera: " << cameraId << std::endl;

        // Acquire the camera
        int ret = camera_->acquire();
        if (ret)
        {
            logger_->error("Failed to acquire camera");
            return false;
        }

        isCameraOpen_ = true;

        return isCameraOpen_;
    } catch (const std::exception &e) {
        logger_->error("Exception in openCamera: " + std::string(e.what()));
        return false;
    }
}

bool GSCameraBase::initializeCamera()
{
    if (!isCameraOpen_)
    {
        logger_->error("Camera not open, cannot initialize");
        return false;
    }

    if (!configureCamera())
    {
        logger_->error("Failed to configure camera during initialization");
        return false;
    }

    if (!configureTriggerMode(triggerMode_))
    {
        logger_->error("Failed to configure trigger mode");
        return false;
    }

    return true;
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
        requests_.clear();

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
        allocator_.reset();

        camera_->release();
        camera_.reset();
    }

    isCameraOpen_ = false;
    isConfigured_ = false;

    // Attempt to give IPA processes time to cleanup after libcamera shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

cv::Mat GSCameraBase::captureFrame()
{
    if (triggerMode_ == TriggerMode::EXTERNAL_TRIGGER)
    {
        // In external trigger mode, just return the latest available frame
        return getLatestFrame();
    }
    else
    {
        // Original free-running implementation
        if (!isCameraOpen_ || !isConfigured_)
        {
            logger_->error("Camera not open or configured");
            return cv::Mat();
        }

        try {
            if (!cameraStarted_)
            {
                int ret = camera_->start();
                if (ret)
                {
                    logger_->error("Failed to start camera");
                    return cv::Mat();
                }
                cameraStarted_ = true;

                for (auto &request : requests_)
                {
                    camera_->queueRequest(request.get());
                }
            }

            std::unique_lock<std::mutex> lock(frameMutex_);
            frameCondition_.wait_for(lock, std::chrono::milliseconds(1000), [this] {
                    return frameReady_;
                });

            if (frameReady_)
            {
                frameReady_ = false;
                return latestFrame_.clone();
            }
            else
            {
                logger_->error("Timeout waiting for frame in captureFrame()");
            }

            return cv::Mat();
        } catch (const std::exception &e) {
            logger_->error("Exception in captureFrame: " + std::string(e.what()));
            return cv::Mat();
        }
    }
}

cv::Mat GSCameraBase::getNextFrame()
{
    return captureFrame(); // For this implementation, same as captureFrame
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

bool GSCameraBase::startContinuousCapture()
{
    if (!isCameraOpen_ || !isConfigured_)
    {
        logger_->error("Camera not open or configured");
        return false;
    }

    if (isCapturing_)
    {
        return true; // Already capturing
    }

    try {
        // Start camera if not running
        if (!cameraStarted_)
        {
            int ret = camera_->start();
            if (ret)
            {
                logger_->error("Failed to start camera");
                return false;
            }
            cameraStarted_ = true;
        }

        // Queue initial requests
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
    isCapturing_ = false;

    if (cameraStarted_)
    {
        camera_->stop();
        cameraStarted_ = false;
    }

    return true;
}

bool GSCameraBase::switchStream(StreamType newStream)
{
    if (activeStream_ == newStream)
    {
        return true; // Already using this stream
    }

    logger_->info("Switching from stream " + std::to_string(static_cast<int>(activeStream_))
                  + " to stream " + std::to_string(static_cast<int>(newStream)));

    bool wasCapturing = isCapturing_;

    // Step 1: Completely stop capture and camera
    if (wasCapturing)
    {
        logger_->info("Stopping continuous capture...");
        isCapturing_ = false;

        // Wait for pending requests to complete
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (cameraStarted_)
    {
        int ret = camera_->stop();
        if (ret)
        {
            logger_->error("Failed to stop camera: " + std::to_string(ret));
            return false;
        }
        cameraStarted_ = false;
        logger_->info("Camera stopped for stream switch");
    }

    // Step 2: Disconnect callback and clear requests
    camera_->requestCompleted.disconnect(this, &GSCameraBase::requestComplete);
    requests_.clear();

    // Step 3: Free ALL buffers
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
                logger_->info("Freed buffers for stream " + std::to_string(i));
            }
        }
        allocator_.reset();
    }

    // Step 4: Switch active stream
    activeStream_ = newStream;

    // Step 5: Create new single-stream configuration for the active stream
    if (!reconfigureForActiveStream())
    {
        logger_->error("Failed to reconfigure for active stream");
        return false;
    }

    logger_->info("Successfully switched to stream " +
                  std::to_string(static_cast<int>(activeStream_)));

    // Step 6: Resume capture if it was running
    if (wasCapturing)
    {
        logger_->info("Restarting capture for new stream...");
        return startContinuousCapture();
    }

    return true;
}

cv::Mat GSCameraBase::getLatestFrame()
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);

    if (frameBuffer_.empty())
    {
        return cv::Mat(); // No frames available
    }

    // Get the most recent frame (discard older ones if multiple available)
    cv::Mat latestFrame;
    while (!frameBuffer_.empty())
    {
        latestFrame = frameBuffer_.front();
        frameBuffer_.pop();
    }

    return latestFrame;
}

std::vector<cv::Mat> GSCameraBase::getAllAvailableFrames()
{
    std::lock_guard<std::mutex> lock(frameBufferMutex_);

    std::vector<cv::Mat> frames;
    while (!frameBuffer_.empty())
    {
        frames.push_back(frameBuffer_.front());
        frameBuffer_.pop();
    }

    return frames;
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

bool GSCameraBase::configureCamera()
{
    return reconfigureForActiveStream();
}

bool GSCameraBase::allocateBuffersForStream(libcamera::Stream *stream)
{
    if (!allocator_)
    {
        logger_->error("Allocator not initialized");
        return false;
    }

    int ret = allocator_->allocate(stream);
    if (ret < 0)
    {
        logger_->error("Failed to allocate buffers for stream");
        return false;
    }

    size_t allocated = allocator_->buffers(stream).size();
    logger_->info("Allocated " + std::to_string(allocated) + " buffers for stream " +
                  std::to_string(static_cast<int>(activeStream_)));

    // Clear any existing requests
    requests_.clear();

    const std::vector<std::unique_ptr<libcamera::FrameBuffer> > &buffers =
        allocator_->buffers(stream);
    for (unsigned int i = 0; i < buffers.size(); ++i)
    {
        std::unique_ptr<libcamera::Request> request = camera_->createRequest();
        if (!request)
        {
            logger_->error("Failed to create request");
            return false;
        }

        ret = request->addBuffer(stream, buffers[i].get());
        if (ret < 0)
        {
            logger_->error("Failed to add buffer to request");
            return false;
        }

        // Set controls (exposure, gain, etc.)
        libcamera::ControlList &controls = request->controls();
        controls.set(libcamera::controls::ExposureTime, currentExposureUs_);
        controls.set(libcamera::controls::AnalogueGain, currentGain_);

        requests_.push_back(std::move(request));
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

cv::Mat GSCameraBase::convertBufferToMat(libcamera::FrameBuffer *buffer)
{
    // Active stream is always at index 0 in single-stream configuration
    const libcamera::StreamConfiguration &streamConfig = config_->at(0);

    const libcamera::FrameBuffer::Plane &plane = buffer->planes()[0];
    void *data = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);

    if (data == MAP_FAILED)
    {
        logger_->error("Failed to map buffer");
        return cv::Mat();
    }

    cv::Mat result;
    int width = streamConfig.size.width;
    int height = streamConfig.size.height;
    size_t stride = streamConfig.stride;

    if (streamConfig.pixelFormat == libcamera::formats::SRGGB10_CSI2P)
    {
        cv::Mat rawImg = unpack10BitBayer(data, width, height, stride);
        cv::cvtColor(rawImg, result, cv::COLOR_BayerRG2BGR);
    }
    else if (streamConfig.pixelFormat == libcamera::formats::BGR888)
    {
        cv::Mat bgrImg(height, width, CV_8UC3, data, stride);
        result = bgrImg.clone();
    }
    else
    {
        logger_->error("Unsupported pixel format: " + streamConfig.pixelFormat.toString());
    }

    munmap(data, plane.length);
    return result;
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
    if (request->status() == libcamera::Request::RequestComplete)
    {
        // Since we only have one stream active at a time, it's always at index
        // 0
        libcamera::FrameBuffer *buffer = request->findBuffer(config_->at(0).stream());
        if (buffer)
        {
            // logger_->info("Received frame from stream " +
            // std::to_string(static_cast<int>(activeStream_)));

            cv::Mat frame = convertBufferToMat(buffer);

            if (triggerMode_ == TriggerMode::EXTERNAL_TRIGGER && isCapturing_)
            {
                addFrameToBuffer(frame);
            }
            else
            {
                {
                    std::lock_guard<std::mutex> lock(frameMutex_);
                    latestFrame_ = frame;
                    frameReady_ = true;
                }
                frameCondition_.notify_one();
            }
        }
        else
        {
            logger_->error("Failed to find buffer for completed request");
        }

        if (isCapturing_)
        {
            request->reuse(libcamera::Request::ReuseBuffers);
            camera_->queueRequest(request);
        }
    }
    else
    {
        logger_->error("Request completed with error status: " + std::to_string(request->status()));
    }
}

cv::Mat GSCameraBase::unpack10BitBayer(void *data, int width, int height, size_t stride)
{
    // SRGGB10_CSI2P packs 4 pixels (40 bits) into 5 bytes
    cv::Mat result(height, width, CV_16UC1);  // Use 16-bit for 10-bit data

    uint8_t *src = static_cast<uint8_t *>(data);
    uint16_t *dst = reinterpret_cast<uint16_t *>(result.data);

    for (int y = 0; y < height; y++)
    {
        uint8_t *row_src = src + y * stride;
        uint16_t *row_dst = dst + y * width;

        for (int x = 0; x < width; x += 4)
        {
            // Unpack 4 pixels from 5 bytes
            int pixels_remaining = std::min(4, width - x);

            if (pixels_remaining >= 1)
            {
                row_dst[x] = (row_src[0] << 2) | ((row_src[4] >> 0) & 0x03);
            }
            if (pixels_remaining >= 2)
            {
                row_dst[x + 1] = (row_src[1] << 2) | ((row_src[4] >> 2) & 0x03);
            }
            if (pixels_remaining >= 3)
            {
                row_dst[x + 2] = (row_src[2] << 2) | ((row_src[4] >> 4) & 0x03);
            }
            if (pixels_remaining >= 4)
            {
                row_dst[x + 3] = (row_src[3] << 2) | ((row_src[4] >> 6) & 0x03);
            }

            row_src += 5; // Move to next 5-byte group
        }
    }

    // Convert to 8-bit for OpenCV compatibility (shift right by 2 bits)
    cv::Mat result8bit;
    result.convertTo(result8bit, CV_8UC1, 1.0 / 4.0);  // Divide by 4 to convert
                                                       // 10-bit to 8-bit

    return result8bit;
}

bool GSCameraBase::reconfigureForActiveStream()
{
    // Generate configuration for ONLY the active stream
    libcamera::StreamRole role;
    switch (activeStream_)
    {
        case StreamType::STREAM_TYPE_PREVIEW:
            role = libcamera::StreamRole::Viewfinder;
            break;
        case StreamType::STREAM_TYPE_MAIN:
            role = libcamera::StreamRole::VideoRecording;
            break;
        case StreamType::STREAM_TYPE_HQ:
            role = libcamera::StreamRole::Raw;
            break;
        default:
            logger_->error("Invalid stream type");
            return false;
    }

    // Generate configuration for single stream
    config_ = camera_->generateConfiguration({role});
    if (!config_)
    {
        logger_->error("Failed to generate configuration for stream " +
                       std::to_string(static_cast<int>(activeStream_)));
        return false;
    }

    // Configure the single stream
    libcamera::StreamConfiguration &streamConfig = config_->at(0); // Only one
                                                                   // stream now

    switch (activeStream_)
    {
        case StreamType::STREAM_TYPE_PREVIEW:
            streamConfig.size.width = resolutionX_ / 2;   // 728
            streamConfig.size.height = resolutionY_ / 2;  // 544
            streamConfig.pixelFormat = libcamera::formats::BGR888;
            break;
        case StreamType::STREAM_TYPE_MAIN:
            streamConfig.size.width = resolutionX_;       // 1456
            streamConfig.size.height = resolutionY_;      // 1088
            streamConfig.pixelFormat = libcamera::formats::BGR888;
            break;
        case StreamType::STREAM_TYPE_HQ:
            streamConfig.size.width = resolutionX_;       // 1456
            streamConfig.size.height = resolutionY_;      // 1088
            streamConfig.pixelFormat = libcamera::formats::SRGGB10_CSI2P;
            break;
    }

    // Validate configuration
    libcamera::CameraConfiguration::Status validation = config_->validate();
    if (validation == libcamera::CameraConfiguration::Invalid)
    {
        logger_->error("Stream configuration invalid");
        return false;
    }

    // Apply configuration
    int ret = camera_->configure(config_.get());
    if (ret)
    {
        logger_->error("Failed to configure camera for stream " +
                       std::to_string(static_cast<int>(activeStream_)));
        return false;
    }

    logger_->info("Configured stream " + std::to_string(static_cast<int>(activeStream_)) + ": "
                  + std::to_string(streamConfig.size.width) + "x" +
                  std::to_string(streamConfig.size.height)
                  + "-" + streamConfig.pixelFormat.toString());

    // Create new allocator
    allocator_ = std::make_unique<libcamera::FrameBufferAllocator>(camera_);
    if (!allocator_)
    {
        logger_->error("Failed to create frame buffer allocator");
        return false;
    }

    // Allocate buffers for the active stream (index 0 since it's the only
    // stream)
    if (!allocateBuffersForStream(config_->at(0).stream()))
    {
        logger_->error("Failed to allocate buffers for active stream");
        return false;
    }

    // Reconnect callback
    camera_->requestCompleted.connect(this, &GSCameraBase::requestComplete);

    isConfigured_ = true;

    return true;
}
} // namespace PiTrac