#include "Infrastructure/Messaging/Messages/GSCameraFrameRawMsg.h"

namespace PiTrac
{
void GSCameraFrameRawMessage::serialize(msgpack::sbuffer &buffer) const
{
    msgpack::packer<msgpack::sbuffer> packer(buffer);

    // Serialize cv::Mat as raw pixel data
    std::vector<uint8_t> raw_frame_data;
    serializeMatRawToBuffer(frame_, raw_frame_data);

    // Get timestamp as milliseconds
    auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp_.time_since_epoch()).count();
    auto capture_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        capture_timestamp_.time_since_epoch()).count();

    // Pack as array: [type, timestamp, camera_id, frame_number,
    // capture_timestamp, fps,
    //                 rows, cols, cv_type, channels, raw_data]
    packer.pack_array(11);
    packer.pack(getMessageType());
    packer.pack(timestamp_ms);
    packer.pack(camera_id_);
    packer.pack(frame_number_);
    packer.pack(capture_timestamp_ms);
    packer.pack(fps_);
    packer.pack(rows_);
    packer.pack(cols_);
    packer.pack(type_);
    packer.pack(channels_);
    packer.pack(raw_frame_data);
}

void GSCameraFrameRawMessage::deserialize(const char *data, size_t size)
{
    msgpack::object_handle oh = msgpack::unpack(data, size);
    msgpack::object obj = oh.get();

    if (obj.type != msgpack::type::ARRAY || obj.via.array.size != 11)
    {
        throw std::runtime_error("Invalid GSCameraFrameRawMessage format");
    }

    // Unpack fields
    std::string message_type;
    int64_t timestamp_ms;
    int64_t capture_timestamp_ms;
    std::vector<uint8_t> raw_frame_data;

    obj.via.array.ptr[0].convert(message_type);
    obj.via.array.ptr[1].convert(timestamp_ms);
    obj.via.array.ptr[2].convert(camera_id_);
    obj.via.array.ptr[3].convert(frame_number_);
    obj.via.array.ptr[4].convert(capture_timestamp_ms);
    obj.via.array.ptr[5].convert(fps_);
    obj.via.array.ptr[6].convert(rows_);
    obj.via.array.ptr[7].convert(cols_);
    obj.via.array.ptr[8].convert(type_);
    obj.via.array.ptr[9].convert(channels_);
    obj.via.array.ptr[10].convert(raw_frame_data);

    if (message_type != getMessageType())
    {
        throw std::runtime_error("Message type mismatch: expected " + getMessageType() +
                                 ", got " + message_type);
    }

    // Restore timestamps
    timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(timestamp_ms));
    capture_timestamp_ = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(capture_timestamp_ms));

    // Deserialize cv::Mat from raw buffer
    frame_ = deserializeMatRawFromBuffer(raw_frame_data, rows_, cols_, type_);
}

std::unique_ptr<GSMessageInterface> GSCameraFrameRawMessage::clone() const
{
    auto cloned = std::make_unique<GSCameraFrameRawMessage>(camera_id_, frame_, frame_number_);
    cloned->timestamp_ = timestamp_;
    cloned->capture_timestamp_ = capture_timestamp_;
    cloned->fps_ = fps_;
    return cloned;
}

void GSCameraFrameRawMessage::serializeMatRawToBuffer(const cv::Mat &mat,
                                                      std::vector<uint8_t> &buffer) const
{
    if (mat.empty())
    {
        buffer.clear();
        return;
    }

    // Ensure the Mat is continuous in memory for efficient copying
    cv::Mat continuous_mat;
    if (mat.isContinuous())
    {
        continuous_mat = mat;
    }
    else
    {
        continuous_mat = mat.clone();
    }

    // Copy raw pixel data
    size_t data_size = continuous_mat.total() * continuous_mat.elemSize();
    buffer.resize(data_size);
    std::memcpy(buffer.data(), continuous_mat.data, data_size);
}

cv::Mat GSCameraFrameRawMessage::deserializeMatRawFromBuffer(const std::vector<uint8_t> &buffer,
                                                             int rows, int cols, int type) const
{
    if (buffer.empty() || rows <= 0 || cols <= 0)
    {
        return cv::Mat();
    }

    // Create Mat with the specified dimensions and type
    cv::Mat mat(rows, cols, type);

    // Verify buffer size matches expected data size
    size_t expected_size = mat.total() * mat.elemSize();
    if (buffer.size() != expected_size)
    {
        throw std::runtime_error("Buffer size mismatch: expected " + std::to_string(expected_size) +
                                 " bytes, got " + std::to_string(buffer.size()) + " bytes");
    }

    // Copy raw data into Mat
    std::memcpy(mat.data, buffer.data(), buffer.size());

    return mat;
}

std::string GSCameraFrameRawMessage::toString() const
{
    std::ostringstream oss;
    oss << GSMessageBase::toString()
        << ", Camera: " << camera_id_
        << ", Frame: " << frame_number_
        << ", Size: " << cols_ << "x" << rows_
        << ", Type: " << type_
        << ", Channels: " << channels_
        << ", FPS: " << fps_
        << ", DataSize: " << getDataSize() << " bytes"
        << ", Empty: " << (isEmpty() ? "true" : "false");
    return oss.str();
}
} // namespace PiTrac