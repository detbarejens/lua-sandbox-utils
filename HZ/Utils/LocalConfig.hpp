#pragma once
#include <string>
#include <vector>
#include <map>

namespace LocalConfig
{
    enum ConfigType
    {
        TYPE_CAMP = 0,
        TYPE_RAGE = 1,
        TYPE_LEGIT = 2,
        TYPE_ROLEPLAY = 3
    };

    struct ConfigInfo
    {
        std::string name;
        ConfigType type;
        std::string filename;
        std::string date;
    };

    // Initialize config system
    void Initialize();

    // Save current settings to a config
    bool SaveConfig(const std::string& configName, ConfigType type, std::string& outMsg);

    // Load settings from a config
    bool LoadConfig(const std::string& filename, std::string& outMsg);

    // Delete a config
    bool DeleteConfig(const std::string& filename, std::string& outMsg);

    // Get list of all configs
    std::vector<ConfigInfo> GetConfigList();

    // Get config / data directory (E:\Mello\Data\)
    std::string GetConfigDirectory();

    // Deployed client directory (E:\Mello\Client\)
    std::string GetClientDirectory();

    // Delete all data in the Mello folder (Clean Traces)
    bool CleanDataDirectory();

    // Encrypt/Decrypt data
    std::string EncryptData(const std::string& data);
    std::string DecryptData(const std::string& data);
}
