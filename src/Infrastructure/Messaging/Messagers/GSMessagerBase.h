#ifndef ZMQ_MESSENGER_H
#define ZMQ_MESSENGER_H

#include "Infrastructure/Messaging/GSMessageInterface.h"
#include <zmq.h>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>

namespace PiTrac {

class ZmqMessenger {
private:
    void* context_;
    void* socket_;
    std::atomic<bool> running_;
    std::thread receive_thread_;
    std::function<void(std::unique_ptr<IMessage>)> message_handler_;
    
public:
    enum class SocketType {
        Publisher,
        Subscriber,
        Request,
        Reply,
        Push,
        Pull
    };
    
    ZmqMessenger(SocketType type) : context_(nullptr), socket_(nullptr), running_(false) {
        context_ = zmq_ctx_new();
        if (!context_) {
            throw std::runtime_error("Failed to create ZMQ context");
        }
        
        int socket_type;
        switch (type) {
            case SocketType::Publisher: socket_type = ZMQ_PUB; break;
            case SocketType::Subscriber: socket_type = ZMQ_SUB; break;
            case SocketType::Request: socket_type = ZMQ_REQ; break;
            case SocketType::Reply: socket_type = ZMQ_REP; break;
            case SocketType::Push: socket_type = ZMQ_PUSH; break;
            case SocketType::Pull: socket_type = ZMQ_PULL; break;
            default: throw std::invalid_argument("Invalid socket type");
        }
        
        socket_ = zmq_socket(context_, socket_type);
        if (!socket_) {
            zmq_ctx_destroy(context_);
            throw std::runtime_error("Failed to create ZMQ socket");
        }
    }
    
    ~ZmqMessenger() {
        stop();
        if (socket_) {
            zmq_close(socket_);
        }
        if (context_) {
            zmq_ctx_destroy(context_);
        }
    }
    
    void bind(const std::string& endpoint) {
        int rc = zmq_bind(socket_, endpoint.c_str());
        if (rc != 0) {
            throw std::runtime_error("Failed to bind to " + endpoint + ": " + zmq_strerror(errno));
        }
    }
    
    void connect(const std::string& endpoint) {
        int rc = zmq_connect(socket_, endpoint.c_str());
        if (rc != 0) {
            throw std::runtime_error("Failed to connect to " + endpoint + ": " + zmq_strerror(errno));
        }
    }
    
    void subscribe(const std::string& topic = "") {
        int rc = zmq_setsockopt(socket_, ZMQ_SUBSCRIBE, topic.c_str(), topic.length());
        if (rc != 0) {
            throw std::runtime_error("Failed to subscribe to topic: " + zmq_strerror(errno));
        }
    }
    
    void sendMessage(const IMessage& message) {
        zmq_msg_t msg;
        message.toZmqMessage(msg);
        
        int rc = zmq_msg_send(&msg, socket_, 0);
        if (rc < 0) {
            zmq_msg_close(&msg);
            throw std::runtime_error("Failed to send message: " + std::string(zmq_strerror(errno)));
        }
        
        zmq_msg_close(&msg);
    }
    
    void sendMessage(const IMessage& message, const std::string& topic) {
        // Send topic frame first
        zmq_msg_t topic_msg;
        zmq_msg_init_size(&topic_msg, topic.size());
        memcpy(zmq_msg_data(&topic_msg), topic.c_str(), topic.size());
        zmq_msg_send(&topic_msg, socket_, ZMQ_SNDMORE);
        zmq_msg_close(&topic_msg);
        
        // Send message frame
        sendMessage(message);
    }
    
    template<typename MessageType>
    std::unique_ptr<MessageType> receiveMessage(int timeout_ms = -1) {
        zmq_msg_t msg;
        zmq_msg_init(&msg);
        
        // Set receive timeout if specified
        if (timeout_ms >= 0) {
            zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));
        }
        
        int rc = zmq_msg_recv(&msg, socket_, 0);
        if (rc < 0) {
            zmq_msg_close(&msg);
            if (errno == EAGAIN) {
                return nullptr; // Timeout
            }
            throw std::runtime_error("Failed to receive message: " + std::string(zmq_strerror(errno)));
        }
        
        auto message = std::make_unique<MessageType>();
        message->fromZmqMessage(msg);
        zmq_msg_close(&msg);
        
        return message;
    }
    
    void startReceiving(std::function<void(std::unique_ptr<IMessage>)> handler) {
        message_handler_ = handler;
        running_ = true;
        receive_thread_ = std::thread([this]() {
            receiveLoop();
        });
    }
    
    void stop() {
        running_ = false;
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
    }
    
private:
    void receiveLoop() {
        while (running_) {
            zmq_msg_t msg;
            zmq_msg_init(&msg);
            
            int timeout = 100; // 100ms timeout
            zmq_setsockopt(socket_, ZMQ_RCVTIMEO, &timeout, sizeof(timeout));
            
            int rc = zmq_msg_recv(&msg, socket_, 0);
            if (rc >= 0 && message_handler_) {
                // Here you'd need message type detection logic
                // For now, this is a placeholder
                zmq_msg_close(&msg);
            } else if (errno != EAGAIN) {
                zmq_msg_close(&msg);
                break; // Error occurred
            }
            zmq_msg_close(&msg);
        }
    }
};

} // namespace PiTrac

#endif // ZMQ_MESSENGER_H