#include "ConfigManager.h"

#include <fstream>

#include "myc/logging/logging.h"

namespace
{
    std::string Trim(const std::string& Value)
    {
        const size_t Start = Value.find_first_not_of(" \t\r\n");
        if (Start == std::string::npos)
        {
            return "";
        }
        const size_t End = Value.find_last_not_of(" \t\r\n");
        return Value.substr(Start, End - Start + 1);
    }
}

ConfigManager& ConfigManager::Get()
{
    static ConfigManager Instance;
    return Instance;
}

bool ConfigManager::LoadFromFile(const std::string& Filename)
{
    std::ifstream File(Filename);
    if (!File.is_open())
    {
        LogWarning("Could not open config file \"%s\", falling back to defaults\n", Filename.c_str());
        return false;
    }

    std::string Line;
    while (std::getline(File, Line))
    {
        //strip comments
        const size_t CommentPos = Line.find_first_of("#;");
        if (CommentPos != std::string::npos)
        {
            Line = Line.substr(0, CommentPos);
        }

        const size_t EqualsPos = Line.find('=');
        if (EqualsPos == std::string::npos)
        {
            continue;
        }

        const std::string Key = Trim(Line.substr(0, EqualsPos));
        if (Key.empty())
        {
            continue;
        }

        Values[Key] = Trim(Line.substr(EqualsPos + 1));
    }

    LogInfo("Loaded config file \"%s\"\n", Filename.c_str());
    return true;
}

int ConfigManager::GetInt(const std::string& Key, int DefaultValue) const
{
    const auto It = Values.find(Key);
    if (It == Values.end())
    {
        return DefaultValue;
    }

    try
    {
        return std::stoi(It->second);
    }
    catch (...)
    {
        LogWarning("Config key \"%s\" has non-integer value \"%s\", using default\n", Key.c_str(), It->second.c_str());
        return DefaultValue;
    }
}

std::string ConfigManager::GetString(const std::string& Key, const std::string& DefaultValue) const
{
    const auto It = Values.find(Key);
    return It == Values.end() ? DefaultValue : It->second;
}
