---
description: "Use when writing, reviewing, or refactoring C++ code in the PiTrac project. Covers error handling, coding standards, and best practices."
applyTo: ["src/**/*.cpp", "src/**/*.h"]
---

# PiTrac C++ Coding Standards

## Error Handling - No Exceptions Policy

**CRITICAL**: This project uses return-type based error handling. Do NOT use C++ exceptions.

### Prohibited Practices
- ❌ `throw` statements
- ❌ `try-catch` blocks for new code
- ❌ `noexcept(false)` specifications
- ❌ Exception-based RAII patterns

### Required Practices

#### 1. Use Return Types for Error Indication

**Preferred Option A: Custom Result/Status types**
```cpp
enum class StatusCode {
    Success,
    FileNotFound,
    ParseError,
    InvalidInput,
    HardwareError
};

struct Result {
    StatusCode status;
    std::string error_message;
    
    bool isOk() const { return status == StatusCode::Success; }
    bool isFailed() const { return status != StatusCode::Success; }
};

// For operations returning data, use templated Result
template<typename T>
struct ResultWith {
    StatusCode status;
    std::string error_message;
    T data;
    
    bool isOk() const { return status == StatusCode::Success; }
};

// Usage
ResultWith<BallPosition> detectBall(const cv::Mat& frame) {
    if (frame.empty()) {
        LOG_ERROR("Invalid frame provided");
        return {StatusCode::InvalidInput, "Empty frame", {}};
    }
    // ... processing
    return {StatusCode::Success, "", BallPosition{x, y, radius}};
}

// Caller checks immediately
auto result = detectBall(frame);
if (result.isFailed()) {
    LOG_ERROR("Detection failed: {}", result.error_message);
    return result.status;
}
processPosition(result.data);
```

**Preferred Option B: Boolean return + output parameter**
```cpp
// Good: Clear success/failure, error details via out param
bool loadConfiguration(const std::string& path, Config& config, std::string& error) {
    if (!std::filesystem::exists(path)) {
        error = "Configuration file not found: " + path;
        return false;
    }
    // ... load config
    return true;
}

// Usage with immediate error checking
Config config;
std::string error;
if (!loadConfiguration(path, config, error)) {
    LOG_ERROR(error);
    return StatusCode::ConfigError;
}
// Continue with valid config
```

**Alternative: `std::optional<T>` for nullable results**
```cpp
// Acceptable for simple cases where no error detail is needed
std::optional<int> findIndex(const std::vector<int>& data, int value) {
    auto it = std::find(data.begin(), data.end(), value);
    if (it == data.end()) {
        return std::nullopt;
    }
    return std::distance(data.begin(), it);
}
```

**Do NOT use `std::expected`** - While targeting C++23, avoid `std::expected<T, E>` in this codebase.

#### 2. Check Errors Immediately

```cpp
// Good: Immediate check and early return
auto result = performOperation();
if (!result) {
    LOG_ERROR("Operation failed");
    return ErrorCode;
}

// Bad: Deferred error checking
auto result = performOperation();
// ... other code ...
if (!result) { /* too late */ }
```

#### 3. Propagate Errors Up the Call Stack

```cpp
// Good: Propagate error state to caller
std::optional<Data> loadAndProcessData(const std::string& path) {
    auto raw_data = loadRawData(path);
    if (!raw_data.has_value()) {
        return std::nullopt;  // Propagate failure
    }
    
    auto processed = processData(raw_data.value());
    if (!processed.has_value()) {
        return std::nullopt;  // Propagate failure
    }
    
    return processed;
}
```

### Existing Exception Code

**Migration in progress**: Some legacy code (particularly in `src/Infrastructure/Messaging/`) still uses exceptions. This is being migrated incrementally:
- **New code**: Always use return types, never exceptions
- **Modifying legacy code**: Prefer refactoring to return types when practical
- **Exception boundaries**: When interfacing with exception-based code, convert to return types at the boundary

### Third-Party Libraries

Some libraries (OpenCV, Boost, ZMQ) may throw exceptions:
- Catch exceptions only at **architectural boundaries** (not in business logic)
- Convert to return codes immediately
- Log exceptional conditions before converting

```cpp
// Good: Catch at boundary, convert to return type
bool safeLoadImage(const std::string& path, cv::Mat& output, std::string& error) {
    try {
        output = cv::imread(path);  // OpenCV may throw
        if (output.empty()) {
            error = "Failed to load image or image is empty";
            return false;
        }
        return true;
    } catch (const cv::Exception& e) {
        error = "OpenCV exception: " + std::string(e.what());
        LOG_ERROR(error);
        return false;
    }
}
```

## Resource Management (RAII Preferred)

### Use RAII for Resource Lifetime Management

**Preferred**: Let destructors handle cleanup automatically
```cpp
// Good: RAII with smart pointers
std::unique_ptr<Camera> camera = createCamera();
// Automatic cleanup when scope exits

// Good: RAII with custom class
class FileHandle {
    int fd_;
public:
    FileHandle(const std::string& path) : fd_(open(path.c_str(), O_RDWR)) {}
    ~FileHandle() { if (fd_ >= 0) close(fd_); }
    // ... methods ...
};
```

### Manual Resource Management

**Allowed** when:
- Resource lifetime is **deterministic** and **function-scoped**
- There's a **specific technical reason** (performance, C API requirements, etc.)
- The acquire/release pattern is clear and contained

```cpp
// Acceptable: Manual management with clear scope
bool processFile(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    
    bool success = readAndProcess(fd);
    
    close(fd);  // Deterministic cleanup before return
    return success;
}
```

**Not acceptable**: Resources with unclear lifetimes or complex control flow
```cpp
// Bad: Manual resource with early returns
bool complexOperation(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    
    if (checkCondition1()) return false;  // fd leaked!
    if (checkCondition2()) return false;  // fd leaked!
    
    close(fd);
    return true;
}
```

### Rationale
- **Predictable performance**: No exception unwinding overhead
- **Explicit error paths**: Clear control flow, easier debugging
- **Embedded-friendly**: Better for real-time systems (Raspberry Pi target)
- **Deterministic behavior**: Essential for hardware control and timing-sensitive operations