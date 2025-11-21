#include "Common/Utils/Json/jsonparser.h"
#include <iostream>

namespace PiTrac
{
AppConfig JsonParser::parseAppConfig(const std::string &filePath, const std::string &key)
{
    AppConfig config;
    parseAppConfigInto(config, filePath, key);
    return config;
}

void JsonParser::parseAppConfigInto(AppConfig &config, const std::string &filePath, const std::string &key)
{
    // Read and parse JSON file
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open JSON file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors))
    {
        throw std::runtime_error("Failed to parse JSON: " + errors);
    }

    // Check if the specified key exists
    if (!root.isMember(key))
    {
        throw std::invalid_argument("Key '" + key + "' not found in JSON file");
    }

    const Json::Value &executables = root[key];
    if (!executables.isObject())
    {
        throw std::runtime_error("Key '" + key + "' is not a JSON object");
    }

    // Clear existing executables
    config.Executables.clear();

    // Parse each executable entry
    for (const auto &execName : executables.getMemberNames())
    {
        const Json::Value &execData = executables[execName];

        try
        {
            AppConfig::ExecConfig execConfig = parseExecConfig(execData);
            config.Executables[execName] = execConfig;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Warning: Failed to parse executable '" << execName
                      << "': " << e.what() << std::endl;
            // Continue parsing other executables
        }
    }
}

AppConfig::ExecConfig JsonParser::parseExecConfig(const Json::Value &execJson)
{
    AppConfig::ExecConfig config;

    // Default values
    config.Enable = true; // Default to enabled if not specified
    config.Executable = "";
    config.Arguments.clear();

    if (!execJson.isObject())
    {
        throw std::runtime_error("Executable configuration is not a JSON object");
    }

    // Parse Enable field (optional, defaults to true)
    if (execJson.isMember("Enable"))
    {
        if (execJson["Enable"].isBool())
        {
            config.Enable = execJson["Enable"].asBool();
        }
        else
        {
            throw std::runtime_error("'Enable' field must be a boolean");
        }
    }

    // Parse Executable field (required)
    if (execJson.isMember("Executable"))
    {
        if (execJson["Executable"].isString())
        {
            config.Executable = execJson["Executable"].asString();
        }
        else
        {
            throw std::runtime_error("'Executable' field must be a string");
        }
    }
    else
    {
        throw std::runtime_error("'Executable' field is required");
    }

    // Parse Arguments field (optional)
    if (execJson.isMember("Arguments"))
    {
        const Json::Value &args = execJson["Arguments"];
        if (args.isArray())
        {
            config.Arguments.clear();
            for (const auto &arg : args)
            {
                if (arg.isString())
                {
                    config.Arguments.push_back(arg.asString());
                }
                else
                {
                    throw std::runtime_error("All arguments must be strings");
                }
            }
        }
        else
        {
            throw std::runtime_error("'Arguments' field must be an array");
        }
    }

    // Parse ProcessOptions field (optional)
    if (execJson.isMember("ProcessOptions"))
    {
        config.processOpts = parseProcessOptions(execJson["ProcessOptions"]);
    }

    return config;
}

AppConfig::ProcessOptions JsonParser::parseProcessOptions(const Json::Value &optsJson)
{
    AppConfig::ProcessOptions opts;

    if (!optsJson.isObject())
    {
        return opts; // Return empty options if not an object
    }

    // Parse processName
    if (optsJson.isMember("processName") && optsJson["processName"].isString())
    {
        opts.processName = optsJson["processName"].asString();
    }

    // Parse cpuAffinity
    if (optsJson.isMember("cpuAffinity") && optsJson["cpuAffinity"].isArray())
    {
        std::vector<int> affinity;
        for (const auto &cpu : optsJson["cpuAffinity"])
        {
            if (cpu.isInt())
            {
                affinity.push_back(cpu.asInt());
            }
        }
        if (!affinity.empty())
        {
            opts.cpuAffinity = affinity;
        }
    }

    // Parse priority
    if (optsJson.isMember("priority") && optsJson["priority"].isInt())
    {
        opts.priority = optsJson["priority"].asInt();
    }

    // Parse schedPolicy
    if (optsJson.isMember("schedPolicy") && optsJson["schedPolicy"].isString())
    {
        opts.schedPolicy = optsJson["schedPolicy"].asString();
    }

    // Parse schedPriority
    if (optsJson.isMember("schedPriority") && optsJson["schedPriority"].isInt())
    {
        opts.schedPriority = optsJson["schedPriority"].asInt();
    }

    // Parse dumpable
    if (optsJson.isMember("dumpable") && optsJson["dumpable"].isBool())
    {
        opts.dumpable = optsJson["dumpable"].asBool();
    }

    // Parse keepCaps
    if (optsJson.isMember("keepCaps") && optsJson["keepCaps"].isBool())
    {
        opts.keepCaps = optsJson["keepCaps"].asBool();
    }

    // Parse deathSignal
    if (optsJson.isMember("deathSignal") && optsJson["deathSignal"].isInt())
    {
        opts.deathSignal = optsJson["deathSignal"].asInt();
    }

    // Parse workingDirectory
    if (optsJson.isMember("workingDirectory") && optsJson["workingDirectory"].isString())
    {
        opts.workingDirectory = optsJson["workingDirectory"].asString();
    }

    // Parse environment
    if (optsJson.isMember("environment") && optsJson["environment"].isObject())
    {
        std::map<std::string, std::string> env;
        for (const auto &key : optsJson["environment"].getMemberNames())
        {
            const Json::Value &value = optsJson["environment"][key];
            if (value.isString())
            {
                env[key] = value.asString();
            }
        }
        if (!env.empty())
        {
            opts.environment = env;
        }
    }

    return opts;
}

CalConfig JsonParser::parseCalConfig(const std::string &filePath)
{
    // Read and parse JSON file
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open JSON file: " + filePath);
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;

    if (!Json::parseFromStream(builder, file, &root, &errors))
    {
        throw std::runtime_error("Failed to parse JSON: " + errors);
    }

    CalConfig config;
    // Parse SchemaVersion
    if (root.isMember("SchemaVersion") && root["SchemaVersion"].isString())
    {
        std::string versionStr = root["SchemaVersion"].asString();
        size_t dotPos = versionStr.find('.');
        if (dotPos != std::string::npos)
        {
            config.SchemaVersionMajor = std::stoul(versionStr.substr(0, dotPos));
            config.SchemaVersionMinor = std::stoul(versionStr.substr(dotPos + 1));
        }
        else
        {
            config.SchemaVersionMajor = std::stoul(versionStr);
            config.SchemaVersionMinor = 0;
        }
    }
    else
    {
        throw std::runtime_error("'SchemaVersion' field is required and must be a string");
    }

    // Parse SchemaFile
    if (root.isMember("SchemaFile") && root["SchemaFile"].isString())
    {
        config.SchemaFile = root["SchemaFile"].asString();
    }
    else
    {
        throw std::runtime_error("'SchemaFile' field is required and must be a string");
    }

    // Parse DatabaseFile
    if (root.isMember("DatabaseFile") && root["DatabaseFile"].isString())
    {
        config.DatabaseFile = root["DatabaseFile"].asString();
    }
    else
    {
        throw std::runtime_error("'DatabaseFile' field is required and must be a string");
    }

    return config;
}
} // namespace PiTrac
