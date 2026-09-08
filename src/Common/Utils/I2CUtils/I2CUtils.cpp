#include "I2CUtils.h"
#include <filesystem>
#include <fstream>
#include <regex>
#include <iostream>
#include <climits>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>
#include <errno.h>
// Suppress OpenSSL 3.0 deprecation warnings for MD5
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/md5.h>
#pragma GCC diagnostic pop

namespace PiTrac
{
int I2CUtils::mapDeviceTreePathToI2CBus(const std::string &deviceTreePath)
{
    // Extract I2C controller address from device tree path
    // Example: /base/axi/pcie@1000120000/rp1/i2c@80000 -> "80000"
    std::regex controllerRegex(R"(i2c@([0-9a-fA-F]+))");
    std::smatch match;

    if (!std::regex_search(deviceTreePath, match, controllerRegex))
    {
        return -1; // No I2C controller found in path
    }

    std::string controllerAddr = match[1].str();

    // Search through all I2C adapters to find matching controller
    try {
        for (const auto &entry : std::filesystem::directory_iterator("/sys/class/i2c-dev/"))
        {
            std::string busName = entry.path().filename();

            // Skip non-i2c entries
            if (busName.substr(0, 4) != "i2c-")
            {
                continue;
            }

            // Extract bus number
            int busNum = std::stoi(busName.substr(4));

            // Check device tree path for this bus
            std::string ofNodePath = entry.path() / "device" / "of_node";

            if (std::filesystem::exists(ofNodePath) && std::filesystem::is_symlink(ofNodePath))
            {
                // Read the symlink target to get device tree path
                char resolvedPath[PATH_MAX];
                ssize_t len = readlink(ofNodePath.c_str(), resolvedPath, sizeof(resolvedPath) - 1);

                if (len != -1)
                {
                    resolvedPath[len] = '\0';
                    std::string dtPath = std::string(resolvedPath);

                    // Check if this device tree path contains our controller
                    // address
                    if (dtPath.find(controllerAddr) != std::string::npos)
                    {
                        return busNum;
                    }
                }
            }
        }
    }
    catch (const std::exception &e) {
        // Handle filesystem errors gracefully
        return -1;
    }

    return -1; // No matching bus found
}

std::vector<int> I2CUtils::findAllI2CBusesForController(const std::string &deviceTreePath)
{
    std::vector<int> buses;

    // Extract I2C controller address from device tree path
    std::regex controllerRegex(R"(i2c@([0-9a-fA-F]+))");
    std::smatch match;

    if (!std::regex_search(deviceTreePath, match, controllerRegex))
    {
        return buses; // No I2C controller found in path
    }

    std::string controllerAddr = match[1].str();

    // Search through all I2C adapters to find matching controllers
    try {
        for (const auto &entry : std::filesystem::directory_iterator("/sys/class/i2c-dev/"))
        {
            std::string busName = entry.path().filename();

            // Skip non-i2c entries
            if (busName.substr(0, 4) != "i2c-")
            {
                continue;
            }

            // Extract bus number
            int busNum = std::stoi(busName.substr(4));

            // Check device tree path for this bus
            std::string ofNodePath = entry.path() / "device" / "of_node";

            if (std::filesystem::exists(ofNodePath) && std::filesystem::is_symlink(ofNodePath))
            {
                // Read the symlink target to get device tree path
                char resolvedPath[PATH_MAX];
                ssize_t len = readlink(ofNodePath.c_str(), resolvedPath, sizeof(resolvedPath) - 1);

                if (len != -1)
                {
                    resolvedPath[len] = '\0';
                    std::string dtPath = std::string(resolvedPath);

                    // Check if this device tree path contains our controller
                    // address
                    if (dtPath.find(controllerAddr) != std::string::npos)
                    {
                        buses.push_back(busNum);
                    }
                }
            }
        }
    }
    catch (const std::exception &e) {
        // Handle filesystem errors gracefully
        buses.clear();
    }

    return buses;
}

uint8_t I2CUtils::extractI2CAddressFromPath(const std::string &deviceTreePath)
{
    // Extract I2C device address from device tree path
    // Example: /base/axi/pcie@1000120000/rp1/i2c@80000/imx296@1a -> 0x1a
    std::regex addrRegex(R"(@([0-9a-fA-F]+)$)");
    std::smatch match;

    if (std::regex_search(deviceTreePath, match, addrRegex))
    {
        return static_cast<uint8_t>(std::stoul(match[1].str(), nullptr, 16));
    }

    return 0; // Invalid address
}

std::string I2CUtils::getI2CDevicePath(int busNumber)
{
    return "/dev/i2c-" + std::to_string(busNumber);
}

std::vector<uint8_t> I2CUtils::readEEPROM(int busNumber, uint8_t deviceAddr,
                                          uint16_t startAddr, size_t length)
{
    std::vector<uint8_t> data;

    std::string devicePath = getI2CDevicePath(busNumber);
    int fd = open(devicePath.c_str(), O_RDWR);

    if (fd < 0)
    {
        return data; // Failed to open device
    }

    if (ioctl(fd, I2C_SLAVE, deviceAddr) < 0)
    {
        close(fd);
        return data; // Failed to set device address
    }

    // Try different EEPROM addressing methods
    bool address_set = false;

    // Method 1: Single byte addressing (for small EEPROMs)
    if (startAddr <= 0xFF)
    {
        uint8_t addr_byte = static_cast<uint8_t>(startAddr);
        ssize_t write_result = write(fd, &addr_byte, 1);
        if (write_result == 1)
        {
            address_set = true;
        }
    }

    // Method 2: Two-byte addressing (for larger EEPROMs)
    if (!address_set)
    {
        uint8_t addr_bytes[2] = {
            static_cast<uint8_t>(startAddr >> 8),   // High byte
            static_cast<uint8_t>(startAddr & 0xFF)  // Low byte
        };

        ssize_t write_result = write(fd, addr_bytes, 2);
        if (write_result == 2)
        {
            address_set = true;
        }
    }

    // Small delay for EEPROM to process address
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    // Read the data
    data.resize(length);
    ssize_t bytes_read = read(fd, data.data(), length);

    close(fd);

    if (bytes_read <= 0)
    {
        data.clear(); // Failed to read
        return data;
    }

    if (bytes_read != static_cast<ssize_t>(length))
    {
        data.resize(bytes_read); // Resize to actual bytes read
    }

    return data;
}

bool I2CUtils::writeEEPROM(int busNumber, uint8_t deviceAddr,
                           uint16_t startAddr, const std::vector<uint8_t> &data)
{
    if (data.empty())
    {
        return false;
    }

    std::string devicePath = getI2CDevicePath(busNumber);

    // Write each byte individually to handle EEPROM timing requirements
    bool success = true;

    for (size_t i = 0; i < data.size() && success; i++)
    {
        uint16_t addr = startAddr + i;
        uint8_t byte_value = data[i];

        // Open fresh connection for each byte to avoid timing issues
        int fd = open(devicePath.c_str(), O_RDWR);

        if (fd < 0)
        {
            success = false;
            break;
        }

        if (ioctl(fd, I2C_SLAVE, deviceAddr) < 0)
        {
            close(fd);
            success = false;
            break;
        }

        // Try single-byte addressing first (most compatible)
        uint8_t write_buffer[2];
        size_t write_size;

        if (addr <= 0xFF)
        {
            // Single-byte addressing
            write_buffer[0] = addr & 0xFF;
            write_buffer[1] = byte_value;
            write_size = 2;
        }
        else
        {
            // This shouldn't happen for our 40-byte UID, but handle it
            close(fd);
            success = false;
            break;
        }

        ssize_t written = write(fd, write_buffer, write_size);

        if (written != static_cast<ssize_t>(write_size))
        {
            success = false;
        }

        close(fd);

        if (success)
        {
            // EEPROM write cycle time - longer delay for reliability
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    if (success)
    {
        // Additional delay after complete write
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return success;
}

std::string I2CUtils::readEEPROMString(int busNumber, uint8_t deviceAddr,
                                       uint16_t startAddr, size_t maxLength)
{
    std::vector<uint8_t> data = readEEPROM(busNumber, deviceAddr, startAddr, maxLength);

    if (data.empty())
    {
        return "";
    }

    // Convert to string directly, stopping at null terminator
    std::string result;
    for (uint8_t byte : data)
    {
        if (byte == 0)
        {
            break; // Stop at null terminator
        }
        result += static_cast<char>(byte);
    }

    return result;
}

bool I2CUtils::writeEEPROMString(int busNumber, uint8_t deviceAddr,
                                 uint16_t startAddr, const std::string &data)
{
    std::vector<uint8_t> bytes(data.begin(), data.end());
    bytes.push_back(0); // Null terminator

    return writeEEPROM(busNumber, deviceAddr, startAddr, bytes);
}

std::string I2CUtils::generateAndStoreCameraUID(int busNumber, uint8_t deviceAddr,
                                                const std::string &deviceTreePath)
{
    // Check if UID already exists
    std::string existing_uid = readStoredCameraUID(busNumber, deviceAddr);
    if (!existing_uid.empty())
    {
        return existing_uid; // Already has a UID
    }

    // Generate seed data for MD5
    std::stringstream seed_data;
    seed_data << deviceTreePath;
    seed_data << "-BUS" << busNumber;
    seed_data << "-ADDR" << std::hex << (int)deviceAddr;
    seed_data << "-TIME" << std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string seed_string = seed_data.str();
    unsigned char md5_result[MD5_DIGEST_LENGTH];

    // Generate MD5 hash of the seed string to create a unique identifier
    #pragma GCC diagnostic push
    // Suppress deprecation warnings for MD5, which is not ideal but serves our
    // purpose for a unique ID
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    MD5(reinterpret_cast<const unsigned char *>(seed_string.c_str()),
        static_cast<unsigned long>(seed_string.length()), md5_result);
    #pragma GCC diagnostic pop

    // Convert to hex string
    std::stringstream uid_stream;
    uid_stream << "CAM-";
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        uid_stream << std::hex << std::setfill('0') << std::setw(2)
                   << static_cast<int>(md5_result[i]);
    }

    std::string uid = uid_stream.str();

    // Store in EEPROM at address EEPROM_UUID_START_ADDR
    if (writeEEPROMString(busNumber, deviceAddr, EEPROM_UUID_START_ADDR, uid))
    {
        return uid;
    }

    return ""; // Failed to store
}

std::string I2CUtils::readStoredCameraUID(int busNumber, uint8_t deviceAddr)
{
    // Read from EEPROM address EEPROM_UUID_START_ADDR where we store the UID
    std::string uid = readEEPROMString(busNumber, deviceAddr, EEPROM_UUID_START_ADDR, EEPROM_UUID_MAX_LENGTH); // MD5
                                                                                                               // +
                                                                                                               // prefix

    // Validate format (should start with "CAM-")
    if (uid.length() >= 36 && uid.substr(0, 4) == "CAM-")
    {
        return uid;
    }

    return ""; // No valid UID found
}
} // namespace PiTrac