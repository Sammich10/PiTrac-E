#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>

namespace PiTrac
{
class SharedMemory
{
  public:
    SharedMemory
    (
        const std::string &name,
        std::size_t size
    );
    ~SharedMemory();

    void *getAddress() const;
    std::size_t getSize() const;

  private:
    std::string name_;
    std::size_t size_;
    boost::interprocess::shared_memory_object shared_memory_;
    boost::interprocess::mapped_region mapped_region_;
};
} // namespace PiTrac

#endif // SHARED_MEMORY_H