//
// simple key=value config file loader
//
#pragma once

#include <string>
#include <unordered_map>

class ConfigManager
{
public:
    static ConfigManager& Get();

    //loads config values from the given file, overwriting any previously loaded value for the same key.
    //missing/unreadable files are not fatal - callers fall back to the defaults passed to GetInt/GetString
    bool LoadFromFile(const std::string& Filename);

    int GetInt(const std::string& Key, int DefaultValue) const;
    std::string GetString(const std::string& Key, const std::string& DefaultValue) const;

private:
    ConfigManager() = default;

    std::unordered_map<std::string, std::string> Values;
};
