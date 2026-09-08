#ifndef __I2C_UTILS_H__
#define __I2C_UTILS_H__

#include <string>
#include <vector>
#include <cstdint>

namespace PiTrac
{
// Assuming we want to store a unique camera ID (UUID) in the EEPROM, we can
// define the following constants:
// UUID Is stored starting at address 0x00, with a maximum length of 40 bytes
// (enough for "CAM-" + 32 hex chars + null terminator)
static constexpr uint16_t EEPROM_UUID_START_ADDR = 0x00;
// Maximum length for the UUID string (including null terminator)
static constexpr size_t EEPROM_UUID_MAX_LENGTH = 40;
// 8 byte calibration identifier stored 4 bytes after the UUID
static constexpr uint16_t EEPROM_CALIB_ID_ADDR = EEPROM_UUID_START_ADDR + EEPROM_UUID_MAX_LENGTH;
// Size of the calibration identifier (64 bit integer value)
static constexpr size_t EEPROM_CALIB_ID_LENGTH = 8;
// Total size of the EEPROM space we will use (less than total EEPROM storage)
static constexpr size_t EEPROM_SIZE_BYTES = 0xFF;

class I2CUtils
{
  public:
    /**
     * @brief Maps a libcamera device tree path to the corresponding I2C bus
     * number
     *
     * Takes a device tree path like "/base/axi/pcie@1000120000/rp1/i2c@80000"
     * and finds the corresponding /dev/i2c-X device by matching the controller
     * address.
     *
     * @param deviceTreePath The device tree path from libcamera camera->id()
     * @return I2C bus number (e.g., 13 for /dev/i2c-13), or -1 if not found
     */
    static int mapDeviceTreePathToI2CBus
    (
        const std::string &deviceTreePath
    );

    /**
     * @brief Finds all I2C buses that match the given device tree controller
     *
     * Some controllers may have multiple I2C buses, this returns all matches.
     *
     * @param deviceTreePath The device tree path from libcamera
     * @return Vector of I2C bus numbers
     */
    static std::vector<int> findAllI2CBusesForController
    (
        const std::string &deviceTreePath
    );

    /**
     * @brief Extracts the I2C device address from a device tree path
     *
     * Takes a path like "/base/axi/pcie@1000120000/rp1/i2c@80000/imx296@1a"
     * and extracts the device address (0x1a in this example).
     *
     * @param deviceTreePath Full device tree path including device address
     * @return I2C device address (e.g., 0x1a), or 0 if not found
     */
    static uint8_t extractI2CAddressFromPath
    (
        const std::string &deviceTreePath
    );

    /**
     * @brief Converts I2C bus number to device path
     *
     * @param busNumber I2C bus number (e.g., 13)
     * @return Device path (e.g., "/dev/i2c-13")
     */
    static std::string getI2CDevicePath
    (
        int busNumber
    );

    // EEPROM Management Functions

    /**
     * @brief Reads data from I2C EEPROM
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address (e.g., 0x50)
     * @param startAddr Starting address in EEPROM to read from
     * @param length Number of bytes to read
     * @return Vector of bytes read, empty if failed
     */
    static std::vector<uint8_t> readEEPROM
    (
        int busNumber,
        uint8_t deviceAddr,
        uint16_t startAddr,
        size_t length
    );

    /**
     * @brief Writes data to I2C EEPROM
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address (e.g., 0x50)
     * @param startAddr Starting address in EEPROM to write to
     * @param data Data to write
     * @return True if successful, false otherwise
     */
    static bool writeEEPROM
    (
        int busNumber,
        uint8_t deviceAddr,
        uint16_t startAddr,
        const std::vector<uint8_t> &data
    );

    /**
     * @brief Reads a null-terminated string from EEPROM
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address
     * @param startAddr Starting address in EEPROM
     * @param maxLength Maximum string length to read
     * @return String read from EEPROM, empty if failed
     */
    static std::string readEEPROMString
    (
        int busNumber,
        uint8_t deviceAddr,
        uint16_t startAddr,
        size_t maxLength = 32
    );

    /**
     * @brief Writes a string to EEPROM (null-terminated)
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address
     * @param startAddr Starting address in EEPROM
     * @param data String to write
     * @return True if successful, false otherwise
     */
    static bool writeEEPROMString
    (
        int busNumber,
        uint8_t deviceAddr,
        uint16_t startAddr,
        const std::string &data
    );

    /**
     * @brief Generates and writes unique camera ID to EEPROM
     *
     * Creates MD5 hash from device info and timestamps, writes to EEPROM.
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address (default 0x50)
     * @param deviceTreePath Device tree path for seed data
     * @return Generated unique ID string, empty if failed
     */
    static std::string generateAndStoreCameraUID
    (
        int busNumber,
        uint8_t deviceAddr,
        const std::string &deviceTreePath
    );

    /**
     * @brief Reads previously stored camera UID from EEPROM
     *
     * @param busNumber I2C bus number
     * @param deviceAddr EEPROM I2C address (default 0x50)
     * @return Stored unique ID string, empty if not found or failed
     */
    static std::string readStoredCameraUID
    (
        int busNumber,
        uint8_t deviceAddr = 0x50
    );
}; // class I2CUtils
}

#endif // __I2C_UTILS_H__