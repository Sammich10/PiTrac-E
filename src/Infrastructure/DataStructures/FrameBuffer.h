#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include <opencv2/opencv.hpp>
#include <queue>
#include <atomic>

namespace PiTrac
{
/**
 * @class FrameBuffer
 * @brief A circular buffer for storing a fixed number of video frames
 *(cv::Mat).
 *
 * The FrameBuffer class implements a circular buffer to efficiently store and
 *manage
 * a sequence of frames. When the buffer reaches its capacity, adding a new
 *frame
 * will overwrite the oldest frame. The buffer supports adding frames,
 *retrieving
 * frames in FIFO order, and querying its state (empty, full, size, capacity).
 *
 * Usage:
 * - Construct with a positive capacity.
 * - Use addFrame() to insert frames; if the buffer is full, the oldest frame is
 *overwritten.
 * - Use getFrame() to retrieve and remove the oldest frame.
 * - Use isEmpty(), isFull(), size(), and capacity() to query buffer status.
 *
 * @note Thread Safety: This class is not thread-safe. The intended use is a
 *single-producer, single-consumer scenario.
 *
 * @note Frames are stored as deep copies (using cv::Mat::clone()) to avoid
 *shared data issues.
 */
class FrameBuffer
{
  public:
    explicit FrameBuffer(const size_t &capacity)
        : capacity_(capacity)
        , frame_buffer_(capacity)
        , head_(0)
        , tail_(0)
    {
        if (capacity_ == 0)
        {
            throw std::invalid_argument("FrameBuffer capacity must be greater than zero.");
        }
    }

    ~FrameBuffer() = default;

    /**
     * @brief Adds a new frame to the circular frame buffer.
     *
     * If the buffer is full, the oldest frame will be overwritten.
     *
     * @param frame The frame to add (as a cv::Mat).
     * @return true if the frame was added without overwriting an existing
     *frame,
     *         false if the oldest frame was overwritten to make space for the
     *new frame.
     */
    bool addFrame(const cv::Mat &frame)
    {
        size_t next_head = (head_ + 1) % capacity_;
        if (next_head == tail_)
        {   // Buffer is full, overwrite the oldest frame
            tail_ = (tail_ + 1) % capacity_;
            // Indicate that an overwrite occurred
            return false;
        }
        // Add the new frame
        frame_buffer_[head_] = frame.clone();
        head_ = next_head;
        return true;
    }

    /**
     * @brief Retrieves the next frame from the buffer.
     *
     * If the buffer is not empty, this function copies the frame at the current
     *tail position
     * into the provided cv::Mat reference, advances the tail index, and returns
     *true.
     * If the buffer is empty, the function returns false and does not modify
     *the frame.
     *
     * @param frame Reference to a cv::Mat object where the retrieved frame will
     *be stored.
     * @return true if a frame was successfully retrieved; false if the buffer
     *is empty.
     */
    bool getFrame(cv::Mat &frame)
    {
        if (isEmpty())
        {
            return false; // Buffer is empty
        }
        frame = frame_buffer_[tail_].clone();
        tail_ = (tail_ + 1) % capacity_;
        return true;
    }

    /**
     * @brief Checks if the frame buffer is empty.
     *
     * @return true if the buffer contains no elements, false otherwise.
     */
    bool isEmpty() const
    {
        return head_ == tail_;
    }

    /**
     * @brief Checks if the frame buffer is full.
     *
     * This function determines whether the buffer has reached its maximum
     *capacity.
     * It returns true if adding another element would overwrite the oldest
     *element,
     * indicating that the buffer is full.
     *
     * @return true if the buffer is full, false otherwise.
     */
    bool isFull() const
    {
        return (tail_ + 1) % capacity_ == head_;
    }

    /**
     * @brief Returns the number of elements currently stored in the buffer.
     *
     * Calculates the size of the circular buffer by determining the distance
     * between the head and tail indices, accounting for wrap-around when the
     * head index is less than the tail index.
     *
     * @return The number of elements in the buffer.
     */
    size_t size() const
    {
        return (head_ >= tail_) ? (head_ - tail_) : (capacity_ - tail_ + head_);
    }

    /**
     * @brief Returns the maximum number of elements the buffer can hold.
     *
     * @return The capacity of the buffer.
     */
    size_t capacity() const
    {
        return capacity_;
    }

  private:
    // Use a circular buffer for efficient memory use and fast access
    std::vector<cv::Mat> frame_buffer_;
    size_t capacity_;
    size_t head_;
    size_t tail_;
};
} // namespace PiTrac

#endif // FRAME_BUFFER_H