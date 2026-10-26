#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <new>
#include <string>
#include <type_traits>

namespace PiTrac
{
/**
 * @brief Base class template that owns a shared memory segment sized to hold exactly one
 * instance of PayloadT, which it placement-constructs/destructs in the mapped region.
 *
 * Derive a type-specific wrapper to expose a strongly-typed shared memory object, e.g.:
 *
 *   struct BallPositionData { double x = 0, y = 0, z = 0; };
 *
 *   class BallPositionSharedMemory : public SharedMemoryObject<BallPositionData>
 *   {
 *     public:
 *       explicit BallPositionSharedMemory(const std::string &name)
 *           : SharedMemoryObject<BallPositionData>(name) {}
 *   };
 *
 * PayloadT must be standard-layout and must not contain virtual functions, raw heap
 * pointers, or process-local containers (std::string, std::vector, etc.): the segment is
 * mapped at an independent address in every attaching process, so a vtable pointer or an
 * absolute pointer written by one process is meaningless to another. Use fixed-size arrays
 * for buffers and embed boost::interprocess::interprocess_mutex/condition members directly
 * in PayloadT for cross-process synchronization.
 */
template <typename PayloadT>
class SharedMemoryObject
{
    static_assert(std::is_standard_layout<PayloadT>::value,
                  "PayloadT stored in shared memory must be standard-layout");

  public:
    explicit SharedMemoryObject(const std::string &name)
        : name_(name)
    {
    }

    ~SharedMemoryObject()
    {
        if (owns_ && data_ != nullptr)
        {
            data_->~PayloadT();
            boost::interprocess::shared_memory_object::remove(name_.c_str());
        }
    }

    SharedMemoryObject
    (
        const SharedMemoryObject &
    ) = delete;
    SharedMemoryObject &operator=
    (
        const SharedMemoryObject &
    ) = delete;

    // Creates a brand-new segment and default-constructs PayloadT in it. Fails if a
    // segment with this name already exists.
    bool create(std::string &error)
    {
        try
        {
            shared_memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::create_only, name_.c_str(), boost::interprocess::read_write);
            shared_memory_.truncate(static_cast<boost::interprocess::offset_t>(sizeof(PayloadT)));
            mapped_region_ = boost::interprocess::mapped_region(shared_memory_, boost::interprocess::read_write);
            data_ = new (mapped_region_.get_address()) PayloadT();
            owns_ = true;
            return true;
        }
        catch (const boost::interprocess::interprocess_exception &e)
        {
            error = "Failed to create shared memory segment '" + name_ + "': " + e.what();
            return false;
        }
    }

    // Opens a segment previously created by another instance/process.
    bool open(std::string &error)
    {
        try
        {
            shared_memory_ = boost::interprocess::shared_memory_object(
                boost::interprocess::open_only, name_.c_str(), boost::interprocess::read_write);
            mapped_region_ = boost::interprocess::mapped_region(shared_memory_, boost::interprocess::read_write);
            data_ = static_cast<PayloadT *>(mapped_region_.get_address());
            owns_ = false;
            return true;
        }
        catch (const boost::interprocess::interprocess_exception &e)
        {
            error = "Failed to open shared memory segment '" + name_ + "': " + e.what();
            return false;
        }
    }

    PayloadT *get()
    {
        return data_;
    }

    const PayloadT *get() const
    {
        return data_;
    }

    std::size_t getSize() const
    {
        return sizeof(PayloadT);
    }

  private:
    std::string name_;
    boost::interprocess::shared_memory_object shared_memory_;
    boost::interprocess::mapped_region mapped_region_;
    PayloadT *data_ = nullptr;
    bool owns_ = false;
};
} // namespace PiTrac

#endif // SHARED_MEMORY_H