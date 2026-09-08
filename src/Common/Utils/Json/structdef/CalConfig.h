#ifndef __CALCONFIG_STRUCT_H__
#define __CALCONFIG_STRUCT_H__

#include <string>
namespace PiTrac
{
/**
 * @brief Configuration for calibration database and schema
 */
struct CalConfig
{
    uint32_t SchemaVersionMajor;
    uint32_t SchemaVersionMinor;
    std::string SchemaFile;
    std::string DatabaseFile;

    CalConfig() = default;
};
} // namespace PiTrac

#endif // __CALCONFIG_STRUCT_H__