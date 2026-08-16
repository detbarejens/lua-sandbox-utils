#include "LocalConfig.hpp"
#include "BrandPaths.hpp"
#include "../Definations/Brand.hpp"
#include "../Definations/Variables.hpp"
#include "../json.hpp"
#include <fstream>
#include <filesystem>
#include <Windows.h>
#include <shlobj.h>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace LocalConfig
{
    static std::string s_ConfigDir;

    std::string GetConfigDirectory()
    {
        if (!s_ConfigDir.empty())
            return s_ConfigDir;

        s_ConfigDir = BrandPaths::GetDataRoot();
        return s_ConfigDir;
    }

    std::string GetClientDirectory()
    {
        const std::string dir = BrandPaths::GetExecutableDirectory();
        if (dir.empty())
            return std::string(BRAND_CLIENT_ROOT);

        if (!fs::exists(dir))
            fs::create_directories(dir);

        return dir;
    }

    // Simple XOR encryption (can be improved with AES)
    std::string EncryptData(const std::string& data)
    {
        std::string encrypted = data;
#if defined(TRINITY_ENCRYPT) && TRINITY_ENCRYPT
        const char key[] = "TrinityBuildKey_0123456789ABCDEF_Slot";
#if defined(TRINITY_BUILD_ID)
        const size_t slot = TRINITY_BUILD_ID;
#else
        const size_t slot = 0;
#endif
        for (size_t i = 0; i < encrypted.size(); i++)
            encrypted[i] ^= key[(i + slot * 7) % (sizeof(key) - 1)];
#else
        const char key[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        const size_t keyLen = strlen(key);
        for (size_t i = 0; i < encrypted.size(); i++)
            encrypted[i] ^= key[i % keyLen];
#endif
        return encrypted;
    }

    std::string DecryptData(const std::string& data)
    {
        return EncryptData(data); // XOR is symmetric
    }

    bool CleanDataDirectory()
    {
        try
        {
            const std::string dir = GetConfigDirectory();
            if (!fs::exists(dir))
                return true;

            fs::remove_all(dir);
            s_ConfigDir.clear();
            s_ConfigDir = BrandPaths::GetDataRoot();
            fs::create_directories(s_ConfigDir);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void Initialize()
    {
        GetConfigDirectory();
    }

    std::string GetCurrentDateTime()
    {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char buffer[64];
        sprintf_s(buffer, "%02d/%02d/%04d %02d:%02d", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
        return std::string(buffer);
    }

    bool SaveConfig(const std::string& configName, ConfigType type, std::string& outMsg)
    {
        try
        {
            json config;
            
            // Save all settings
            config["version"] = "1.0";
            config["name"] = configName;
            config["type"] = (int)type;
            config["date"] = GetCurrentDateTime();
            
            // Aimbot
            config["aimbot"]["enabled"] = g_Options.LegitBot.AimBot.Enabled;
            config["aimbot"]["keybind"] = g_Options.LegitBot.AimBot.KeyBind;
            config["aimbot"]["fov"] = g_Options.LegitBot.AimBot.FOV;
            config["aimbot"]["smoothX"] = g_Options.LegitBot.AimBot.SmoothHorizontal;
            config["aimbot"]["smoothY"] = g_Options.LegitBot.AimBot.SmoothVertical;
            config["aimbot"]["hitbox"] = g_Options.LegitBot.AimBot.HitBox;
            config["aimbot"]["maxDistance"] = g_Options.LegitBot.AimBot.MaxDistance;
            config["aimbot"]["showFov"] = g_Options.LegitBot.AimBot.ShowFov;
            config["aimbot"]["visibleCheck"] = g_Options.LegitBot.AimBot.VisibleCheck;
            config["aimbot"]["fovColor"] = { g_Options.LegitBot.AimBot.FovColor[0], g_Options.LegitBot.AimBot.FovColor[1], g_Options.LegitBot.AimBot.FovColor[2], g_Options.LegitBot.AimBot.FovColor[3] };
            
            // Silent Aim
            config["silent"]["enabled"] = g_Options.LegitBot.SilentAim.Enabled;
            config["silent"]["keybind"] = g_Options.LegitBot.SilentAim.KeyBind;
            config["silent"]["fov"] = g_Options.LegitBot.SilentAim.Fov;
            config["silent"]["hitbox"] = g_Options.LegitBot.SilentAim.HitBox;
            config["silent"]["maxDistance"] = g_Options.LegitBot.SilentAim.MaxDistance;
            config["silent"]["showFov"] = g_Options.LegitBot.SilentAim.ShowFOV;
            config["silent"]["magicBullet"] = g_Options.LegitBot.SilentAim.MagicBullet;
            config["silent"]["fovColor"] = { g_Options.LegitBot.SilentAim.FovColor[0], g_Options.LegitBot.SilentAim.FovColor[1], g_Options.LegitBot.SilentAim.FovColor[2], g_Options.LegitBot.SilentAim.FovColor[3] };

            config["magic"]["enabled"] = g_Options.LegitBot.MagicBullets.Enabled;
            config["magic"]["showFov"] = g_Options.LegitBot.MagicBullets.ShowFOV;
            config["magic"]["fov"] = g_Options.LegitBot.MagicBullets.FOV;
            config["magic"]["fovColor"] = { g_Options.LegitBot.MagicBullets.FovColor[0], g_Options.LegitBot.MagicBullets.FovColor[1], g_Options.LegitBot.MagicBullets.FovColor[2], g_Options.LegitBot.MagicBullets.FovColor[3] };
            
            // Trigger
            config["trigger"]["enabled"] = g_Options.LegitBot.Trigger.Enabled;
            config["trigger"]["keybind"] = g_Options.LegitBot.Trigger.KeyBind;
            config["trigger"]["maxDistance"] = g_Options.LegitBot.Trigger.MaxDistance;
            config["trigger"]["showFov"] = g_Options.LegitBot.Trigger.ShowFOV;
            config["trigger"]["fov"] = g_Options.LegitBot.Trigger.FOV;
            config["trigger"]["fovColor"] = { g_Options.LegitBot.Trigger.FovColor[0], g_Options.LegitBot.Trigger.FovColor[1], g_Options.LegitBot.Trigger.FovColor[2], g_Options.LegitBot.Trigger.FovColor[3] };
            
            // Player ESP
            config["playerESP"]["enabled"] = g_Options.Visuals.Players.Enabled;
            config["playerESP"]["box"] = g_Options.Visuals.Players.EnableBox;
            config["playerESP"]["boxType"] = g_Options.Visuals.Players.BoxType;
            config["playerESP"]["distance"] = g_Options.Visuals.Players.EnableDistance;
            config["playerESP"]["healthBar"] = g_Options.Visuals.Players.HealthBar;
            config["playerESP"]["healthBarType"] = g_Options.Visuals.Players.HealthBarType;
            config["playerESP"]["armorBar"] = g_Options.Visuals.Players.AmorBar;
            config["playerESP"]["armorBarType"] = g_Options.Visuals.Players.AmorBarType;
            config["playerESP"]["skeleton"] = g_Options.Visuals.Players.Skeleton;
            config["playerESP"]["weaponName"] = g_Options.Visuals.Players.WeaponName;
            config["playerESP"]["observerAlert"] = g_Options.Visuals.Players.ObserverAlert;
            config["playerESP"]["observerAlertShowHud"] = g_Options.Visuals.Players.ObserverAlertShowHud;
            config["playerESP"]["observerAlertHudX"] = g_Options.Visuals.Players.ObserverAlertHudX;
            config["playerESP"]["observerAlertHudY"] = g_Options.Visuals.Players.ObserverAlertHudY;
            config["playerESP"]["observerMaxDistance"] = g_Options.Visuals.Players.ObserverMaxDistance;
            config["playerESP"]["observerLookThreshold"] = g_Options.Visuals.Players.ObserverLookThreshold;
            config["playerESP"]["observerCanSeeFov"] = g_Options.Visuals.Players.ObserverCanSeeFov;
            config["playerESP"]["observerVisibleCheck"] = g_Options.Visuals.Players.ObserverVisibleCheck;
            config["playerESP"]["aimingAlert"] = g_Options.Visuals.Players.AimingAlert;
            config["playerESP"]["aimingAlertShowHud"] = g_Options.Visuals.Players.AimingAlertShowHud;
            config["playerESP"]["aimingAlertHudX"] = g_Options.Visuals.Players.AimingAlertHudX;
            config["playerESP"]["aimingAlertHudY"] = g_Options.Visuals.Players.AimingAlertHudY;
            config["playerESP"]["aimingMaxDistance"] = g_Options.Visuals.Players.AimingMaxDistance;
            config["playerESP"]["aimingLookThreshold"] = g_Options.Visuals.Players.AimingLookThreshold;
            config["playerESP"]["aimingAwarenessFov"] = g_Options.Visuals.Players.AimingAwarenessFov;
            config["playerESP"]["aimingVisibleCheck"] = g_Options.Visuals.Players.AimingVisibleCheck;
            
            // Vehicle ESP
            config["vehicleESP"]["enabled"] = g_Options.Visuals.Vehicles.Enabled;
            config["vehicleESP"]["name"] = g_Options.Visuals.Vehicles.Name;
            config["vehicleESP"]["distance"] = g_Options.Visuals.Vehicles.Distance;
            
            // Display
            config["general"]["secondMonitor"] = g_Options.General.SecondMonitor;
            config["general"]["monitorIndex"] = g_Options.General.MonitorIndex;
            config["general"]["streamProof"] = g_Options.General.CaptureBypass;

            // Exploits
            config["exploits"]["godMode"] = g_Options.Exploits.Self.GodMode;
            config["exploits"]["noClip"] = g_Options.Exploits.Self.NoClip;
            config["exploits"]["removeSpread"] = g_Options.Exploits.Self.RemoveSpread;
            config["exploits"]["removeRecoil"] = g_Options.Exploits.Self.RemoveRecoil;
            
            // Serialize to string
            std::string jsonStr = config.dump(2);
            
            // Encrypt
            std::string encrypted = EncryptData(jsonStr);
            
            // Generate discrete filename
            std::string filename = "wsc_" + std::to_string(std::hash<std::string>{}(configName)) + ".tmp";
            std::string fullPath = GetConfigDirectory() + filename;
            
            // Save to file
            std::ofstream file(fullPath, std::ios::binary);
            if (!file.is_open())
            {
                outMsg = "Failed to create config file";
                return false;
            }
            
            file.write(encrypted.c_str(), encrypted.size());
            file.close();
            
            // Hide the file
            SetFileAttributesA(fullPath.c_str(), FILE_ATTRIBUTE_HIDDEN);
            
            outMsg = "Config saved successfully!";
            return true;
        }
        catch (const std::exception& e)
        {
            outMsg = std::string("Error: ") + e.what();
            return false;
        }
    }

    bool LoadConfig(const std::string& filename, std::string& outMsg)
    {
        try
        {
            std::string fullPath = GetConfigDirectory() + filename;
            
            // Read file
            std::ifstream file(fullPath, std::ios::binary);
            if (!file.is_open())
            {
                outMsg = "Config file not found";
                return false;
            }
            
            std::string encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();
            
            // Decrypt
            std::string jsonStr = DecryptData(encrypted);
            
            // Parse JSON
            json config = json::parse(jsonStr);
            
            auto loadColor = [](const json& node, const char* key, float* dest)
            {
                if (!node.contains(key) || !node[key].is_array())
                    return;
                const auto& arr = node[key];
                for (int i = 0; i < 4 && i < (int)arr.size(); ++i)
                    dest[i] = arr[i].get<float>();
            };

            // Load all settings
            if (config.contains("aimbot"))
            {
                g_Options.LegitBot.AimBot.Enabled = config["aimbot"]["enabled"];
                g_Options.LegitBot.AimBot.KeyBind = config["aimbot"]["keybind"];
                g_Options.LegitBot.AimBot.FOV = config["aimbot"]["fov"];
                g_Options.LegitBot.AimBot.SmoothHorizontal = config["aimbot"]["smoothX"];
                g_Options.LegitBot.AimBot.SmoothVertical = config["aimbot"]["smoothY"];
                g_Options.LegitBot.AimBot.HitBox = config["aimbot"]["hitbox"];
                g_Options.LegitBot.AimBot.MaxDistance = config["aimbot"]["maxDistance"];
                g_Options.LegitBot.AimBot.ShowFov = config["aimbot"]["showFov"];
                g_Options.LegitBot.AimBot.VisibleCheck = config["aimbot"]["visibleCheck"];
                loadColor(config["aimbot"], "fovColor", g_Options.LegitBot.AimBot.FovColor);
            }
            
            if (config.contains("silent"))
            {
                g_Options.LegitBot.SilentAim.Enabled = config["silent"]["enabled"];
                g_Options.LegitBot.SilentAim.KeyBind = config["silent"]["keybind"];
                g_Options.LegitBot.SilentAim.Fov = config["silent"]["fov"];
                g_Options.LegitBot.SilentAim.HitBox = config["silent"]["hitbox"];
                g_Options.LegitBot.SilentAim.MaxDistance = config["silent"]["maxDistance"];
                g_Options.LegitBot.SilentAim.ShowFOV = config["silent"]["showFov"];
                g_Options.LegitBot.SilentAim.MagicBullet = config["silent"]["magicBullet"];
                loadColor(config["silent"], "fovColor", g_Options.LegitBot.SilentAim.FovColor);
            }

            if (config.contains("magic"))
            {
                if (config["magic"].contains("enabled"))
                    g_Options.LegitBot.MagicBullets.Enabled = config["magic"]["enabled"];
                if (config["magic"].contains("showFov"))
                    g_Options.LegitBot.MagicBullets.ShowFOV = config["magic"]["showFov"];
                if (config["magic"].contains("fov"))
                    g_Options.LegitBot.MagicBullets.FOV = config["magic"]["fov"];
                loadColor(config["magic"], "fovColor", g_Options.LegitBot.MagicBullets.FovColor);
            }
            
            if (config.contains("trigger"))
            {
                g_Options.LegitBot.Trigger.Enabled = config["trigger"]["enabled"];
                g_Options.LegitBot.Trigger.KeyBind = config["trigger"]["keybind"];
                g_Options.LegitBot.Trigger.MaxDistance = config["trigger"]["maxDistance"];
                if (config["trigger"].contains("showFov"))
                    g_Options.LegitBot.Trigger.ShowFOV = config["trigger"]["showFov"];
                if (config["trigger"].contains("fov"))
                    g_Options.LegitBot.Trigger.FOV = config["trigger"]["fov"];
                loadColor(config["trigger"], "fovColor", g_Options.LegitBot.Trigger.FovColor);
            }
            
            if (config.contains("playerESP"))
            {
                g_Options.Visuals.Players.Enabled = config["playerESP"]["enabled"];
                g_Options.Visuals.Players.EnableBox = config["playerESP"]["box"];
                g_Options.Visuals.Players.BoxType = config["playerESP"]["boxType"];
                g_Options.Visuals.Players.EnableDistance = config["playerESP"]["distance"];
                g_Options.Visuals.Players.HealthBar = config["playerESP"]["healthBar"];
                g_Options.Visuals.Players.HealthBarType = config["playerESP"]["healthBarType"];
                g_Options.Visuals.Players.AmorBar = config["playerESP"]["armorBar"];
                g_Options.Visuals.Players.AmorBarType = config["playerESP"]["armorBarType"];
                g_Options.Visuals.Players.Skeleton = config["playerESP"]["skeleton"];
                g_Options.Visuals.Players.WeaponName = config["playerESP"]["weaponName"];
                auto loadIf = [&](const char* key, auto& dest) {
                    if (config["playerESP"].contains(key))
                        dest = config["playerESP"][key];
                };
                loadIf("observerAlert", g_Options.Visuals.Players.ObserverAlert);
                loadIf("observerAlertShowHud", g_Options.Visuals.Players.ObserverAlertShowHud);
                loadIf("observerAlertHudX", g_Options.Visuals.Players.ObserverAlertHudX);
                loadIf("observerAlertHudY", g_Options.Visuals.Players.ObserverAlertHudY);
                loadIf("observerMaxDistance", g_Options.Visuals.Players.ObserverMaxDistance);
                loadIf("observerLookThreshold", g_Options.Visuals.Players.ObserverLookThreshold);
                loadIf("observerCanSeeFov", g_Options.Visuals.Players.ObserverCanSeeFov);
                loadIf("observerVisibleCheck", g_Options.Visuals.Players.ObserverVisibleCheck);
                loadIf("aimingAlert", g_Options.Visuals.Players.AimingAlert);
                loadIf("aimingAlertShowHud", g_Options.Visuals.Players.AimingAlertShowHud);
                loadIf("aimingAlertHudX", g_Options.Visuals.Players.AimingAlertHudX);
                loadIf("aimingAlertHudY", g_Options.Visuals.Players.AimingAlertHudY);
                loadIf("aimingMaxDistance", g_Options.Visuals.Players.AimingMaxDistance);
                loadIf("aimingLookThreshold", g_Options.Visuals.Players.AimingLookThreshold);
                loadIf("aimingAwarenessFov", g_Options.Visuals.Players.AimingAwarenessFov);
                loadIf("aimingVisibleCheck", g_Options.Visuals.Players.AimingVisibleCheck);
            }

            if (config.contains("general"))
            {
                if (config["general"].contains("secondMonitor"))
                    g_Options.General.SecondMonitor = config["general"]["secondMonitor"];
                if (config["general"].contains("monitorIndex"))
                    g_Options.General.MonitorIndex = config["general"]["monitorIndex"];
                if (config["general"].contains("streamProof"))
                    g_Options.General.CaptureBypass = config["general"]["streamProof"];
            }
            
            if (config.contains("vehicleESP"))
            {
                g_Options.Visuals.Vehicles.Enabled = config["vehicleESP"]["enabled"];
                g_Options.Visuals.Vehicles.Name = config["vehicleESP"]["name"];
                g_Options.Visuals.Vehicles.Distance = config["vehicleESP"]["distance"];
            }
            
            if (config.contains("exploits"))
            {
                g_Options.Exploits.Self.GodMode = config["exploits"]["godMode"];
                g_Options.Exploits.Self.NoClip = config["exploits"]["noClip"];
                g_Options.Exploits.Self.RemoveSpread = config["exploits"]["removeSpread"];
                g_Options.Exploits.Self.RemoveRecoil = config["exploits"]["removeRecoil"];
            }
            
            outMsg = "Config loaded successfully!";
            return true;
        }
        catch (const std::exception& e)
        {
            outMsg = std::string("Error: ") + e.what();
            return false;
        }
    }

    bool DeleteConfig(const std::string& filename, std::string& outMsg)
    {
        try
        {
            std::string fullPath = GetConfigDirectory() + filename;
            
            if (!fs::exists(fullPath))
            {
                outMsg = "Config not found";
                return false;
            }
            
            fs::remove(fullPath);
            outMsg = "Config deleted successfully!";
            return true;
        }
        catch (const std::exception& e)
        {
            outMsg = std::string("Error: ") + e.what();
            return false;
        }
    }

    std::vector<ConfigInfo> GetConfigList()
    {
        std::vector<ConfigInfo> configs;
        
        try
        {
            std::string configDir = GetConfigDirectory();
            
            for (const auto& entry : fs::directory_iterator(configDir))
            {
                if (entry.path().extension() == ".tmp" && 
                    entry.path().filename().string().find("wsc_") == 0)
                {
                    try
                    {
                        // Read and decrypt config to get info
                        std::ifstream file(entry.path(), std::ios::binary);
                        if (!file.is_open()) continue;
                        
                        std::string encrypted((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        file.close();
                        
                        std::string jsonStr = DecryptData(encrypted);
                        json config = json::parse(jsonStr);
                        
                        ConfigInfo info;
                        info.name = config["name"];
                        info.type = (ConfigType)(int)config["type"];
                        info.filename = entry.path().filename().string();
                        info.date = config["date"];
                        
                        configs.push_back(info);
                    }
                    catch (...) { continue; }
                }
            }
        }
        catch (...) {}
        
        return configs;
    }
}
