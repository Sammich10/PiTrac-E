#include "Infrastructure/Messaging/Messages/External/CameraFrameMsg.h"
#include <opencv2/imgcodecs.hpp>

namespace PiTrac
{
void CameraFrameMsg::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    // Serialize cv::Mat to compressed JPEG buffer
    std::vector<uint8_t> encoded_frame;
    serializeMatToBuffer(frame_, encoded_frame);

    // Get timestamp as milliseconds
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    auto capture_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        capture_timestamp_.time_since_epoch()).count();

    // Pack as array: [type, timestamp, camera_id, frame_number,
    // capture_timestamp, fps, frame_data]
    packer.pack_array(7);
    // Pack common fields [type, timestamp]
    packCommonFields(packer);
    packer.pack(camera_id_);
    packer.pack(frame_number_);
    packer.pack(capture_timestamp_ms);
    packer.pack(fps_);
    packer.pack(encoded_frame);
}

void CameraFrameMsg::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 7)
    {
        throw std::runtime_error("Invalid CameraFrameMsg format");
    }

    // Unpack fields
    int message_type;
    int64_t timestamp_ms;
    int64_t capture_timestamp_ms;
    std::vector<uint8_t> encoded_frame;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(camera_id_);
    obj.via.array.ptr[3].convert(frame_number_);
    obj.via.array.ptr[4].convert(capture_timestamp_ms);
    obj.via.array.ptr[5].convert(fps_);
    obj.via.array.ptr[6].convert(encoded_frame);

    if (static_cast<Message_Type>(message_type) != getMessageType())
    {
        throw std::runtime_error(incorrectMessageTypeString(static_cast<Message_Type>(message_type)));
    }

    // Restore timestamps
    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
    capture_timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(capture_timestamp_ms));

    // Deserialize cv::Mat from buffer
    frame_ = deserializeMatFromBuffer(encoded_frame);
}

std::unique_ptr<MessageInterface> CameraFrameMsg::clone() const
{
    auto cloned = std::make_unique<CameraFrameMsg>(camera_id_, frame_, frame_number_);
    cloned->timestamp_ = timestamp_;
    cloned->capture_timestamp_ = capture_timestamp_;
    cloned->fps_ = fps_;
    cloned->jpeg_quality_ = jpeg_quality_;
    return cloned;
}

void CameraFrameMsg::serializeMatToBuffer(const cv::Mat &mat,
                                          std::vector<uint8_t> &buffer) const
{
    if (mat.empty())
    {
        buffer.clear();
        return;
    }

    // Compress as JPEG to reduce message size
    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(jpeg_quality_);

    cv::imencode(".jpg", mat, buffer, compression_params);
}

cv::Mat CameraFrameMsg::deserializeMatFromBuffer(const std::vector<uint8_t> &buffer) const
{
    if (buffer.empty())
    {
        return cv::Mat();
    }

    // Decode JPEG from buffer
    return cv::imdecode(buffer, cv::IMREAD_COLOR);
}

std::string CameraFrameMsg::toString() const
{
    std::ostringstream oss;
    oss << MessageBase::toString()
        << ", Camera: " << camera_id_
        << ", Frame: " << frame_number_
        << ", Size: " << getFrameSize().width << "x" << getFrameSize().height
        << ", FPS: " << fps_
        << ", Type: " << getFrameType()
        << ", Empty: " << (isEmpty() ? "true" : "false");
    return oss.str();
}
} // namespace PiTrac