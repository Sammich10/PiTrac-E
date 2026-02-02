#ifndef __LIBCAMERA_UTILS_H__
#define __LIBCAMERA_UTILS_H__

#include <opencv2/opencv.hpp>
#include <libcamera/libcamera.h>
#include <libcamera/framebuffer_allocator.h>
#include <sys/mman.h>
#include <memory>
#include <cstdint>
#include <cstddef>

namespace PiTrac
{
/**
 * @brief Utility class for camera-related functions, including image data unpacking, 
 * conversion, and frame processing.
 */
class CameraUtils
{
    public:
    /**
     * @brief Unpacks 10-bit Bayer formatted image data into a cv::Mat object.
     *
     * @param data Pointer to the raw 10-bit Bayer image data.
     * @param width Width of the image in pixels.
     * @param height Height of the image in pixels.
     * @param stride Number of bytes per row in the input data.
     *
     * @return cv::Mat The unpacked image as an OpenCV matrix.
     */
    static cv::Mat unpack10BitBayer
    (
        void *data,
        int width,
        int height,
        size_t stride
    );

    /**
     * @brief Converts a libcamera::FrameBuffer to an OpenCV cv::Mat object.
     *
     * @param buffer Pointer to the libcamera::FrameBuffer containing the image data.
     * @param streamConfig The stream configuration associated with the buffer.
     * 
     * @return cv::Mat The resulting OpenCV matrix containing the image.
     */
    static cv::Mat convertBufferToMat
    (
        libcamera::FrameBuffer *buffer,
        const libcamera::StreamConfiguration &streamConfig
    );

}; // class CameraUtils
} // namespace PiTrac

#endif // __LIBCAMERA_UTILS_H__