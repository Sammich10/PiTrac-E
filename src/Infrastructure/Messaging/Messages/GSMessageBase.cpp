#include "Infrastructure/Messaging/Messages/GSMessageBase.h"

namespace PiTrac {

    GSMessageBase::GSMessageBase() 
    : timestamp_(std::chrono::system_clock::now()) 
    {}
    
    // Timestamp operations
    std::chrono::system_clock::time_point GSMessageBase::getTimestamp() const {
        return timestamp_;
    }

    void GSMessageBase::setTimestamp(const std::chrono::system_clock::time_point& timestamp) {
        timestamp_ = timestamp;
    }
    
    // ZMQ message operations implementation
    void GSMessageBase::toZmqMessage(zmq_msg_t& msg) const {
        msgpack::sbuffer buffer;
        serialize(buffer);
        
        int rc = zmq_msg_init_size(&msg, buffer.size());
        if (rc != 0) {
            throw std::runtime_error("Failed to initialize ZMQ message: " + std::string(zmq_strerror(errno)));
        }
        
        memcpy(zmq_msg_data(&msg), buffer.data(), buffer.size());
    }
    
    void GSMessageBase::fromZmqMessage(zmq_msg_t& msg) {
        const char* data = static_cast<const char*>(zmq_msg_data(&msg));
        size_t size = zmq_msg_size(&msg);
        deserialize(data, size);
    }
    
    std::string GSMessageBase::toString() const {
        std::ostringstream oss;
        oss << "Message Type: " << getMessageType() 
            << ", Timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp_.time_since_epoch()).count() << "ms";
        return oss.str();
    }
    
} // namespace PiTrac