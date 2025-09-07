#ifndef GS_CAMERA_FRAME_MESSAGE_H
#define GS_CAMERA_FRAME_MESSAGE_H

#include "Infrastructure/Messaging/Messages/GSMessageBase.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace PiTrac
{
class GSCameraFrameMessage : public GSMessageBase
{
  private:
    cv::Mat frame_;
    std::string camera_id_;
    uint64_t frame_number_;
    std::chrono::system_clock::time_point capture_timestamp_;
    double fps_;

  public:
    GSCameraFrameMessage() = default;

    GSCameraFrameMessage(const std::string &camera_id,
                         const cv::Mat &frame,
                         uint64_t frame_number)
        : camera_id_(camera_id),
        frame_(frame.clone()),
        frame_number_(frame_number),
        capture_timestamp_(std::chrono::system_clock::now()),
        fps_(0.0)
    {
    }

    // GSMessageInterface implementation
    std::string getMessageType() const override
    {
        return "CameraFrame";
    }

    void serialize(msgpack::sbuffer &buffer) const override;
    void deserialize(const char *data, size_t size) override;
    std::unique_ptr<GSMessageInterface> clone() const override;

    // Camera frame specific methods
    const cv::Mat &getFrame() const
    {
        return frame_;
    }

    void setFrame(const cv::Mat &frame)
    {
        frame_ = frame.clone();
    }

    const std::string &getCameraId() const
    {
        return camera_id_;
    }

    void setCameraId(const std::string &camera_id)
    {
        camera_id_ = camera_id;
    }

    uint64_t getFrameNumber() const
    {
        return frame_number_;
    }

    void setFrameNumber(uint64_t frame_number)
    {
        frame_number_ = frame_number;
    }

    std::chrono::system_clock::time_point getCaptureTimestamp() const
    {
        return capture_timestamp_;
    }

    void setCaptureTimestamp(const std::chrono::system_clock::time_point &timestamp)
    {
        capture_timestamp_ = timestamp;
    }

    double getFps() const
    {
        return fps_;
    }

    void setFps(double fps)
    {
        fps_ = fps;
    }

    // Utility methods
    cv::Size getFrameSize() const
    {
        return frame_.size();
    }

    int getFrameType() const
    {
        return frame_.type();
    }

    bool isEmpty() const
    {
        return frame_.empty();
    }

    // Compression options for large frames
    void setJpegQuality(int quality)
    {
        jpeg_quality_ = quality;
    }

    int getJpegQuality() const
    {
        return jpeg_quality_;
    }

    std::string toString() const override;

  private:
    int jpeg_quality_ = 95; // JPEG compression quality (0-100)

    // Helper methods for cv::Mat serialization
    void serializeMatToBuffer(const cv::Mat &mat, std::vector<uint8_t> &buffer) const;
    cv::Mat deserializeMatFromBuffer(const std::vector<uint8_t> &buffer) const;
};
} // namespace PiTrac

#endif // GS_CAMERA_FRAME_MESSAGE_H