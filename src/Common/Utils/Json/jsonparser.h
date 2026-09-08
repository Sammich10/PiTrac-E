#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <json/json.h>
#include "structdef/AppConfig.h"
#include "structdef/CalConfig.h"

namespace PiTrac
{
class JsonParser
{
  public:
    /**
     * Generic JSON to struct converter
     * @param filePath Path to JSON file
     * @param outputStruct Reference to struct to populate
     * @return true if successful
     */
    template <typename T>
    static bool JSONToStruct(const std::string &filePath, T &outputStruct)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            throw std::runtime_error("Could not open JSON file: " + filePath);
        }

        Json::Value root;
        file >> root;

        // Assuming T has a method fromJson(const Json::Value&)
        outputStruct.fromJson(root);
        return true;
    }

    /**
     * Parse AppConfig from JSON file, extracting executables from specified key
     * @param filePath Path to the JSON configuration file
     * @param key The key in JSON to parse (default: "Executables")
     * @return AppConfig struct with parsed executables
     */
    static AppConfig parseAppConfig
    (
        const std::string &filePath,
        const std::string &key = "Executables"
    );

    /**
     * Parse executables into existing AppConfig from JSON file
     * @param config Reference to AppConfig to populate
     * @param filePath Path to the JSON configuration file
     * @param key The key in JSON to parse (default: "Executables")
     */
    static void parseAppConfigInto
    (
        AppConfig &config,
        const std::string &filePath,
        const std::string &key = "Executables"
    );

    /**
     * Parse CalConfig from JSON file
     * @param filePath Path to the JSON configuration file
     * @return CalConfig struct with parsed data
     */
    static CalConfig parseCalConfig
    (
        const std::string &filePath
    );

  private:
    /**
     * Convert JSON Value to ExecConfig struct
     * @param execJson JSON Value containing executable configuration
     * @return ExecConfig struct
     */
    static AppConfig::ExecConfig parseExecConfig
    (
        const Json::Value &execJson
    );

    /**
     * Convert JSON Value to ProcessOptions struct
     * @param optsJson JSON Value containing process options
     * @return ProcessOptions struct
     */
    static AppConfig::ProcessOptions parseProcessOptions
    (
        const Json::Value &optsJson
    );
};
} // namespace PiTrac

#endif // JSON_PARSER_H