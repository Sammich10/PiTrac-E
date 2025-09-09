#ifndef GS_CAMERA_FRAME_RAW_MESSAGE_H
#define GS_CAMERA_FRAME_RAW_MESSAGE_H

#include "Infrastructure/Messaging/Messages/GSMessageBase.h"
#include <opencv2/opencv.hpp>
#include <vector>

namespace PiTrac
{
class GSCameraFrameRawMessage : public GSMessageBase
{
  private:
    cv::Mat frame_;
    std::string camera_id_;
    uint64_t frame_number_;
    std::chrono::system_clock::time_point capture_timestamp_;
    double fps_;

    // Raw frame metadata
    int rows_;
    int cols_;
    int type_;
    int channels_;

  public:
    GSCameraFrameRawMessage() = default;

    GSCameraFrameRawMessage(const std::string &camera_id,
                            const cv::Mat &frame,
                            uint64_t frame_number)
        : camera_id_(camera_id),
        frame_(frame.clone()),
        frame_number_(frame_number),
        capture_timestamp_(std::chrono::system_clock::now()),
        fps_(0.0),
        rows_(frame.rows),
        cols_(frame.cols),
        type_(frame.type()),
        channels_(frame.channels())
    {
    }

    // GSMessageInterface implementation
    std::string getMessageType() const override
    {
        return "CameraFrameRaw";
    }

    void serialize
    (
        msgpack::sbuffer &buffer
    ) const override;
    void deserialize
    (
        const char *data,
        size_t size
    ) override;
    std::unique_ptr<GSMessageInterface> clone() const override;

    // Camera frame specific methods
    const cv::Mat &getFrame() const
    {
        return frame_;
    }

    void setFrame(const cv::Mat &frame)
    {
        frame_ = frame.clone();
        rows_ = frame.rows;
        cols_ = frame.cols;
        type_ = frame.type();
        channels_ = frame.channels();
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

    // Frame metadata methods
    cv::Size getFrameSize() const
    {
        return cv::Size(cols_, rows_);
    }

    int getFrameType() const
    {
        return type_;
    }

    int getChannels() const
    {
        return channels_;
    }

    bool isEmpty() const
    {
        return frame_.empty();
    }

    size_t getDataSize() const
    {
        return frame_.total() * frame_.elemSize();
    }

    std::string toString() const override;

  private:
    // Helper methods for raw cv::Mat serialization (no compression)
    void serializeMatRawToBuffer
    (
        const cv::Mat &mat,
        std::vector<uint8_t> &buffer
    ) const;
    cv::Mat deserializeMatRawFromBuffer
    (
        const std::vector<uint8_t> &buffer,
        int rows,
        int cols,
        int type
    ) const;
};
} // namespace PiTrac

#endif // GS_CAMERA_FRAME_RAW_MESSAGE_H