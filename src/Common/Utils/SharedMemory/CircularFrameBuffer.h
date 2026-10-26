#ifndef CIRCULAR_FRAME_BUFFER_H
#define CIRCULAR_FRAME_BUFFER_H

#include "Common/Utils/SharedMemory/SharedMemory.h"
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <array>
#include <cstddef>
#include <string>
#include <type_traits>

namespace PiTrac
{
/**
 * @brief Fixed-capacity circular buffer of FrameT, designed to be used as the PayloadT of
 * SharedMemoryObject<CircularFrameBuffer<FrameT, Capacity>> so multiple processes can
 * produce/consume frames through the same shared memory segment.
 *
 * FrameT must be standard-layout and self-contained (e.g. a fixed-size pixel buffer struct)
 * since whole frames are copied by value into/out of shared memory slots - no pointers,
 * std::vector, or std::string members.
 */
template <typename FrameT, std::size_t Capacity>
class CircularFrameBuffer
{
    static_assert(Capacity > 0, "Capacity must be greater than zero");
    static_assert(std::is_standard_layout<FrameT>::value, "FrameT must be standard-layout to live in shared memory");

  public:
    CircularFrameBuffer() = default;

    // Non-blocking push. Returns false if the buffer is full.
    bool tryPush(const FrameT &frame)
    {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(mutex_);
        if (count_ == Capacity)
        {
            return false;
        }
        frames_[tail_] = frame;
        tail_ = (tail_ + 1) % Capacity;
        ++count_;
        not_empty_.notify_one();
        return true;
    }

    // Non-blocking pop. Returns false if the buffer is empty.
    bool tryPop(FrameT &frame)
    {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(mutex_);
        if (count_ == 0)
        {
            return false;
        }
        frame = frames_[head_];
        head_ = (head_ + 1) % Capacity;
        --count_;
        not_full_.notify_one();
        return true;
    }

    // Blocks until a slot is free. Returns false only on an interprocess sync error.
    bool push(const FrameT &frame)
    {
        try
        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(mutex_);
            while (count_ == Capacity)
            {
                not_full_.wait(lock);
            }
            frames_[tail_] = frame;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            not_empty_.notify_one();
            return true;
        }
        catch (const boost::interprocess::interprocess_exception &)
        {
            return false;
        }
    }

    // Blocks until a frame is available. Returns false only on an interprocess sync error.
    bool pop(FrameT &frame)
    {
        try
        {
            boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(mutex_);
            while (count_ == 0)
            {
                not_empty_.wait(lock);
            }
            frame = frames_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            not_full_.notify_one();
            return true;
        }
        catch (const boost::interprocess::interprocess_exception &)
        {
            return false;
        }
    }

    std::size_t size() const
    {
        boost::interprocess::scoped_lock<boost::interprocess::interprocess_mutex> lock(mutex_);
        return count_;
    }

    static constexpr std::size_t capacity()
    {
        return Capacity;
    }

  private:
    mutable boost::interprocess::interprocess_mutex mutex_;
    boost::interprocess::interprocess_condition not_empty_;
    boost::interprocess::interprocess_condition not_full_;
    std::array<FrameT, Capacity> frames_{};
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
};

/**
 * @brief Shared-memory-backed circular frame buffer. Thin wrapper around
 * SharedMemoryObject<CircularFrameBuffer<FrameT, Capacity>> that forwards push/pop calls
 * directly to the buffer placed in shared memory.
 */
template <typename FrameT, std::size_t Capacity>
class FrameBufferSharedMemory : public SharedMemoryObject<CircularFrameBuffer<FrameT, Capacity> >
{
  public:
    explicit FrameBufferSharedMemory(const std::string &name)
        : SharedMemoryObject<CircularFrameBuffer<FrameT, Capacity> >(name)
    {
    }

    bool tryPush(const FrameT &frame)
    {
        return this->get()->tryPush(frame);
    }

    bool tryPop(FrameT &frame)
    {
        return this->get()->tryPop(frame);
    }

    bool push(const FrameT &frame)
    {
        return this->get()->push(frame);
    }

    bool pop(FrameT &frame)
    {
        return this->get()->pop(frame);
    }
};
} // namespace PiTrac

#endif // CIRCULAR_FRAME_BUFFER_H
