#include "Common/Utils/CameraUtils/CameraUtils.h"

namespace PiTrac
{
cv::Mat CameraUtils::unpack10BitBayer(void *data, int width, int height, size_t stride)
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
    // 10-bit range (0-1023) should map to 8-bit range (0-255)
    result.convertTo(result8bit, CV_8UC1, 255.0 / 1023.0);

    return result8bit;
}

cv::Mat CameraUtils::convertBufferToMat(libcamera::FrameBuffer *buffer, const libcamera::StreamConfiguration &streamConfig)
{
    const libcamera::FrameBuffer::Plane &plane = buffer->planes()[0];
    void *data = mmap(nullptr, plane.length, PROT_READ, MAP_SHARED, plane.fd.get(), 0);

    if (data == MAP_FAILED)
    {
        // logger_->error("Failed to map buffer");
        return cv::Mat();
    }

    cv::Mat result;
    int width = streamConfig.size.width;
    int height = streamConfig.size.height;
    size_t stride = streamConfig.stride;

    if (streamConfig.pixelFormat == libcamera::formats::SRGGB10_CSI2P)
    {
        cv::Mat rawImg = CameraUtils::unpack10BitBayer(data, width, height, stride);
        cv::cvtColor(rawImg, result, cv::COLOR_BayerRG2BGR);
    }
    else if (streamConfig.pixelFormat == libcamera::formats::BGR888)
    {
        cv::Mat bgrImg(height, width, CV_8UC3, data, stride);
        result = bgrImg.clone();
    }
    else
    {
        // logger_->error("Unsupported pixel format: " +
        // streamConfig.pixelFormat.toString());
    }

    munmap(data, plane.length);
    return result;
}
} // namespace PiTrac