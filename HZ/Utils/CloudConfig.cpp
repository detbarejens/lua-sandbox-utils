#include "CloudConfig.hpp"
#include "../Definations/Variables.hpp"
#include "../Security/XorStr.hpp"
#include "../json.hpp"
#include <Windows.h>
#include <wininet.h>
#include <sstream>
#include <thread>
#include <algorithm>

#pragma comment(lib, "wininet.lib")

using json = nlohmann::json;

// ─── HTTP ─────────────────────────────────────────────────────────────────────
static std::string HttpPost(const std::string& url, const std::string& postData)
{
    std::string response = "";
    try
    {
        HINTERNET hInternet = InternetOpenA(xorstr("CloudConfig/1.0").crypt_get(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return "";

        URL_COMPONENTSA urlComp = { 0 };
        urlComp.dwStructSize = sizeof(urlComp);
        char hostName[256] = { 0 };
        char urlPath[1024] = { 0 };
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = sizeof(hostName);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath);

        InternetCrackUrlA(url.c_str(), 0, 0, &urlComp);

        HINTERNET hConnect = InternetConnectA(hInternet, hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return "";
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            flags |= INTERNET_FLAG_SECURE;
        }

        HINTERNET hRequest = HttpOpenRequestA(hConnect, xorstr("POST").crypt_get(), urlPath, NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string headers = xorstr("Content-Type: application/json\r\n").crypt_get();
        if (HttpSendRequestA(hRequest, headers.c_str(), -1, (LPVOID)postData.c_str(), (DWORD)postData.length())) {
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;
            }
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
    }
    catch (...)
    {
    }
    return response;
}

static void SendWebhookToDiscord(const std::string& webhookUrl, const std::string& url) {
    try {
        // Get current time as timestamp
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::string timestamp = std::to_string(now_c);
        
        json payload = {
            {"embeds", json::array({
                {
                    {"title", "🔗 Remote Control Session Started"},
                    {"description", "A new remote control session has been started."},
                    {"color", 0x00ff00},
                    {"fields", json::array({
                        {{"name", "🔗 Remote URL"}, {"value", url}, {"inline", false}}
                    })},
                    {"timestamp", timestamp},
                    {"footer", {{"text", "Storm Remote Control"}}}
                }
            })},
            {"username", "Storm Remote Control"},
            {"avatar_url", "https://i.imgur.com/logo.png"}
        };
     
        HttpPost(webhookUrl, payload.dump());
    } catch (...) {
        // Ignore errors
    }
}

static std::string HttpGet(const std::string& url)
{
    std::string response = "";
    try
    {
        HINTERNET hInternet = InternetOpenA(xorstr("CloudConfig/1.0").crypt_get(), INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) return "";

        URL_COMPONENTSA urlComp = { 0 };
        urlComp.dwStructSize = sizeof(urlComp);
        char hostName[256] = { 0 };
        char urlPath[1024] = { 0 };
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = sizeof(hostName);
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = sizeof(urlPath);

        InternetCrackUrlA(url.c_str(), 0, 0, &urlComp);

        HINTERNET hConnect = InternetConnectA(hInternet, hostName, urlComp.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect) {
            InternetCloseHandle(hInternet);
            return "";
        }

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (urlComp.nScheme == INTERNET_SCHEME_HTTPS) {
            flags |= INTERNET_FLAG_SECURE;
        }

        HINTERNET hRequest = HttpOpenRequestA(hConnect, xorstr("GET").crypt_get(), urlPath, NULL, NULL, NULL, flags, 0);
        if (!hRequest) {
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return "";
        }

        if (HttpSendRequestA(hRequest, NULL, 0, NULL, 0)) {
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                response += buffer;
            }
        }

        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
    }
    catch (...)
    {
    }
    return response;
}

// ─── CloudConfig ──────────────────────────────────────────────────────────────
namespace CloudConfig
{
    static json BuildJson()
    {
        json config;

        // Aimbot
        config[xorstr("aimbot").crypt_get()][xorstr("enabled").crypt_get()] = g_Options.LegitBot.AimBot.Enabled;
        config[xorstr("aimbot").crypt_get()][xorstr("showFov").crypt_get()] = g_Options.LegitBot.AimBot.ShowFov;
        config[xorstr("aimbot").crypt_get()][xorstr("fov").crypt_get()] = g_Options.LegitBot.AimBot.FOV;
        config[xorstr("aimbot").crypt_get()][xorstr("maxDistance").crypt_get()] = g_Options.LegitBot.AimBot.MaxDistance;
        config[xorstr("aimbot").crypt_get()][xorstr("smoothH").crypt_get()] = g_Options.LegitBot.AimBot.SmoothHorizontal;
        config[xorstr("aimbot").crypt_get()][xorstr("smoothV").crypt_get()] = g_Options.LegitBot.AimBot.SmoothVertical;
        config[xorstr("aimbot").crypt_get()][xorstr("hitBox").crypt_get()] = g_Options.LegitBot.AimBot.HitBox;
        config[xorstr("aimbot").crypt_get()][xorstr("visibleCheck").crypt_get()] = g_Options.LegitBot.AimBot.VisibleCheck;
        config[xorstr("aimbot").crypt_get()][xorstr("targetNPC").crypt_get()] = g_Options.LegitBot.AimBot.TargetNPC;
        config[xorstr("aimbot").crypt_get()][xorstr("strictLock").crypt_get()] = g_Options.LegitBot.AimBot.StrictLock;
        config[xorstr("aimbot").crypt_get()][xorstr("keyBind").crypt_get()] = g_Options.LegitBot.AimBot.KeyBind;
        config[xorstr("aimbot").crypt_get()][xorstr("fovColor").crypt_get()] = { g_Options.LegitBot.AimBot.FovColor[0], g_Options.LegitBot.AimBot.FovColor[1], g_Options.LegitBot.AimBot.FovColor[2], g_Options.LegitBot.AimBot.FovColor[3] };

        // Silent Aim
        config[xorstr("silentAim").crypt_get()][xorstr("enabled").crypt_get()] = g_Options.LegitBot.SilentAim.Enabled;
        config[xorstr("silentAim").crypt_get()][xorstr("showFOV").crypt_get()] = g_Options.LegitBot.SilentAim.ShowFOV;
        config[xorstr("silentAim").crypt_get()][xorstr("fov").crypt_get()] = g_Options.LegitBot.SilentAim.Fov;
        config[xorstr("silentAim").crypt_get()][xorstr("maxDistance").crypt_get()] = g_Options.LegitBot.SilentAim.MaxDistance;
        config[xorstr("silentAim").crypt_get()][xorstr("hitBox").crypt_get()] = g_Options.LegitBot.SilentAim.HitBox;
        config[xorstr("silentAim").crypt_get()][xorstr("missChance").crypt_get()] = g_Options.LegitBot.SilentAim.MissChance;
        config[xorstr("silentAim").crypt_get()][xorstr("visibleCheck").crypt_get()] = g_Options.LegitBot.SilentAim.VisibleCheck;
        config[xorstr("silentAim").crypt_get()][xorstr("shotNPC").crypt_get()] = g_Options.LegitBot.SilentAim.ShotNPC;
        config[xorstr("silentAim").crypt_get()][xorstr("humanize").crypt_get()] = g_Options.LegitBot.SilentAim.Humanize;
        config[xorstr("silentAim").crypt_get()][xorstr("keyBind").crypt_get()] = g_Options.LegitBot.SilentAim.KeyBind;
        config[xorstr("silentAim").crypt_get()][xorstr("fovColor").crypt_get()] = { g_Options.LegitBot.SilentAim.FovColor[0], g_Options.LegitBot.SilentAim.FovColor[1], g_Options.LegitBot.SilentAim.FovColor[2], g_Options.LegitBot.SilentAim.FovColor[3] };

        config[xorstr("magic").crypt_get()][xorstr("enabled").crypt_get()] = g_Options.LegitBot.MagicBullets.Enabled;
        config[xorstr("magic").crypt_get()][xorstr("showFOV").crypt_get()] = g_Options.LegitBot.MagicBullets.ShowFOV;
        config[xorstr("magic").crypt_get()][xorstr("fov").crypt_get()] = g_Options.LegitBot.MagicBullets.FOV;
        config[xorstr("magic").crypt_get()][xorstr("fovColor").crypt_get()] = { g_Options.LegitBot.MagicBullets.FovColor[0], g_Options.LegitBot.MagicBullets.FovColor[1], g_Options.LegitBot.MagicBullets.FovColor[2], g_Options.LegitBot.MagicBullets.FovColor[3] };

        // Trigger Bot
        config[xorstr("triggerBot").crypt_get()][xorstr("enabled").crypt_get()] = g_Options.LegitBot.Trigger.Enabled;
        config[xorstr("triggerBot").crypt_get()][xorstr("maxDistance").crypt_get()] = g_Options.LegitBot.Trigger.MaxDistance;
        config[xorstr("triggerBot").crypt_get()][xorstr("reactionTime").crypt_get()] = g_Options.LegitBot.Trigger.ReactionTime;
        config[xorstr("triggerBot").crypt_get()][xorstr("visibleCheck").crypt_get()] = g_Options.LegitBot.Trigger.VisibleCheck;
        config[xorstr("triggerBot").crypt_get()][xorstr("shotNPC").crypt_get()] = g_Options.LegitBot.Trigger.ShotNPC;
        config[xorstr("triggerBot").crypt_get()][xorstr("keyBind").crypt_get()] = g_Options.LegitBot.Trigger.KeyBind;
        config[xorstr("triggerBot").crypt_get()][xorstr("showFOV").crypt_get()] = g_Options.LegitBot.Trigger.ShowFOV;
        config[xorstr("triggerBot").crypt_get()][xorstr("fov").crypt_get()] = g_Options.LegitBot.Trigger.FOV;
        config[xorstr("triggerBot").crypt_get()][xorstr("fovColor").crypt_get()] = { g_Options.LegitBot.Trigger.FovColor[0], g_Options.LegitBot.Trigger.FovColor[1], g_Options.LegitBot.Trigger.FovColor[2], g_Options.LegitBot.Trigger.FovColor[3] };

        // Visuals - Players
        config[xorstr("visuals").crypt_get()][xorstr("players").crypt_get()] = g_Options.Visuals.Players.Enabled;
        config[xorstr("visuals").crypt_get()][xorstr("renderDistance").crypt_get()] = g_Options.Visuals.Players.RenderDistance;
        config[xorstr("visuals").crypt_get()][xorstr("visibleCheck").crypt_get()] = g_Options.Visuals.Players.VisibleCheck;
        config[xorstr("visuals").crypt_get()][xorstr("box").crypt_get()] = g_Options.Visuals.Players.EnableBox;
        config[xorstr("visuals").crypt_get()][xorstr("skeleton").crypt_get()] = g_Options.Visuals.Players.Skeleton;
        config[xorstr("visuals").crypt_get()][xorstr("healthBar").crypt_get()] = g_Options.Visuals.Players.HealthBar;
        config[xorstr("visuals").crypt_get()][xorstr("distance").crypt_get()] = g_Options.Visuals.Players.EnableDistance;
        config[xorstr("visuals").crypt_get()][xorstr("name").crypt_get()] = g_Options.Visuals.Players.Name;
        config[xorstr("visuals").crypt_get()][xorstr("snapLine").crypt_get()] = g_Options.Visuals.Players.SnapLine;
        config[xorstr("visuals").crypt_get()][xorstr("showLocalPlayer").crypt_get()] = g_Options.Visuals.Players.ShowLocalPlayer;
        config[xorstr("visuals").crypt_get()][xorstr("showNPC").crypt_get()] = g_Options.Visuals.Players.ShowNPC;
        config[xorstr("visuals").crypt_get()][xorstr("excludeDeads").crypt_get()] = g_Options.Visuals.Players.ExcludeDeads;
        config[xorstr("visuals").crypt_get()][xorstr("toggleKey").crypt_get()] = g_Options.Visuals.Players.ToggleKey;
        config[xorstr("visuals").crypt_get()][xorstr("observerAlert").crypt_get()] = g_Options.Visuals.Players.ObserverAlert;
        config[xorstr("visuals").crypt_get()][xorstr("observerAlertShowHud").crypt_get()] = g_Options.Visuals.Players.ObserverAlertShowHud;
        config[xorstr("visuals").crypt_get()][xorstr("observerAlertHudX").crypt_get()] = g_Options.Visuals.Players.ObserverAlertHudX;
        config[xorstr("visuals").crypt_get()][xorstr("observerAlertHudY").crypt_get()] = g_Options.Visuals.Players.ObserverAlertHudY;
        config[xorstr("visuals").crypt_get()][xorstr("observerMaxDistance").crypt_get()] = g_Options.Visuals.Players.ObserverMaxDistance;
        config[xorstr("visuals").crypt_get()][xorstr("observerLookThreshold").crypt_get()] = g_Options.Visuals.Players.ObserverLookThreshold;
        config[xorstr("visuals").crypt_get()][xorstr("observerCanSeeFov").crypt_get()] = g_Options.Visuals.Players.ObserverCanSeeFov;
        config[xorstr("visuals").crypt_get()][xorstr("observerVisibleCheck").crypt_get()] = g_Options.Visuals.Players.ObserverVisibleCheck;
        config[xorstr("visuals").crypt_get()][xorstr("aimingAlert").crypt_get()] = g_Options.Visuals.Players.AimingAlert;
        config[xorstr("visuals").crypt_get()][xorstr("aimingAlertShowHud").crypt_get()] = g_Options.Visuals.Players.AimingAlertShowHud;
        config[xorstr("visuals").crypt_get()][xorstr("aimingAlertHudX").crypt_get()] = g_Options.Visuals.Players.AimingAlertHudX;
        config[xorstr("visuals").crypt_get()][xorstr("aimingAlertHudY").crypt_get()] = g_Options.Visuals.Players.AimingAlertHudY;
        config[xorstr("visuals").crypt_get()][xorstr("aimingMaxDistance").crypt_get()] = g_Options.Visuals.Players.AimingMaxDistance;
        config[xorstr("visuals").crypt_get()][xorstr("aimingLookThreshold").crypt_get()] = g_Options.Visuals.Players.AimingLookThreshold;
        config[xorstr("visuals").crypt_get()][xorstr("aimingAwarenessFov").crypt_get()] = g_Options.Visuals.Players.AimingAwarenessFov;
        config[xorstr("visuals").crypt_get()][xorstr("aimingVisibleCheck").crypt_get()] = g_Options.Visuals.Players.AimingVisibleCheck;

        // Visuals - Vehicles
        config[xorstr("visuals").crypt_get()][xorstr("vehicles").crypt_get()] = g_Options.Visuals.Vehicles.Enabled;
        config[xorstr("visuals").crypt_get()][xorstr("vehicleDistance").crypt_get()] = g_Options.Visuals.Vehicles.Distance;
        config[xorstr("visuals").crypt_get()][xorstr("vehicleName").crypt_get()] = g_Options.Visuals.Vehicles.Name;
        config[xorstr("visuals").crypt_get()][xorstr("vehicleMarker").crypt_get()] = g_Options.Visuals.Vehicles.Marker;

        // Crosshair
        config[xorstr("crosshair").crypt_get()][xorstr("enabled").crypt_get()] = g_Options.Crosshair.Enabled;
        config[xorstr("crosshair").crypt_get()][xorstr("showDot").crypt_get()] = g_Options.Crosshair.ShowDot;
        config[xorstr("crosshair").crypt_get()][xorstr("showLines").crypt_get()] = g_Options.Crosshair.ShowLines;
        config[xorstr("crosshair").crypt_get()][xorstr("showOutline").crypt_get()] = g_Options.Crosshair.ShowOutline;
        config[xorstr("crosshair").crypt_get()][xorstr("dynamicGap").crypt_get()] = g_Options.Crosshair.DynamicGap;
        config[xorstr("crosshair").crypt_get()][xorstr("style").crypt_get()] = g_Options.Crosshair.Style;
        config[xorstr("crosshair").crypt_get()][xorstr("size").crypt_get()] = g_Options.Crosshair.Size;
        config[xorstr("crosshair").crypt_get()][xorstr("gap").crypt_get()] = g_Options.Crosshair.Gap;
        config[xorstr("crosshair").crypt_get()][xorstr("thickness").crypt_get()] = g_Options.Crosshair.Thickness;
        config[xorstr("crosshair").crypt_get()][xorstr("dotSize").crypt_get()] = g_Options.Crosshair.DotSize;

        // Exploits
        config[xorstr("exploits").crypt_get()][xorstr("invisible").crypt_get()] = g_Options.Exploits.Self.Invisible;
        config[xorstr("exploits").crypt_get()][xorstr("godMode").crypt_get()] = g_Options.Exploits.Self.GodMode;
        config[xorstr("exploits").crypt_get()][xorstr("noClip").crypt_get()] = g_Options.Exploits.Self.NoClip;
        config[xorstr("exploits").crypt_get()][xorstr("noRagDoll").crypt_get()] = g_Options.Exploits.Self.NoRagDoll;
        config[xorstr("exploits").crypt_get()][xorstr("removeSpread").crypt_get()] = g_Options.Exploits.Self.RemoveSpread;
        config[xorstr("exploits").crypt_get()][xorstr("removeRecoil").crypt_get()] = g_Options.Exploits.Self.RemoveRecoil;
        config[xorstr("exploits").crypt_get()][xorstr("seatBelt").crypt_get()] = g_Options.Exploits.Self.SeatBelt;
        config[xorstr("exploits").crypt_get()][xorstr("antHS").crypt_get()] = g_Options.Exploits.Self.AntHS;
        config[xorstr("exploits").crypt_get()][xorstr("weaponSize").crypt_get()] = g_Options.Exploits.Self.WeaponSize;
        config[xorstr("exploits").crypt_get()][xorstr("noClipSpeed").crypt_get()] = g_Options.Exploits.Self.NoClipSpeed;

        // General
        config[xorstr("general").crypt_get()][xorstr("vsync").crypt_get()] = g_Options.General.Vsync;
        config[xorstr("general").crypt_get()][xorstr("captureBypass").crypt_get()] = g_Options.General.CaptureBypass;
        config[xorstr("general").crypt_get()][xorstr("menuKey").crypt_get()] = g_Options.General.MenuKey;
        config[xorstr("general").crypt_get()][xorstr("secondMonitor").crypt_get()] = g_Options.General.SecondMonitor;
        config[xorstr("general").crypt_get()][xorstr("monitorIndex").crypt_get()] = g_Options.General.MonitorIndex;

        return config;
    }


    static void ApplyConfig(const json& config)
    {
        try
        {
            if (config.contains(xorstr("aimbot").crypt_get())) {
                auto& ab = config[xorstr("aimbot").crypt_get()];
                if (ab.contains(xorstr("enabled").crypt_get())) g_Options.LegitBot.AimBot.Enabled = ab[xorstr("enabled").crypt_get()];
                if (ab.contains(xorstr("fov").crypt_get())) g_Options.LegitBot.AimBot.FOV = ab[xorstr("fov").crypt_get()];
                if (ab.contains(xorstr("maxDistance").crypt_get())) g_Options.LegitBot.AimBot.MaxDistance = ab[xorstr("maxDistance").crypt_get()];
                if (ab.contains(xorstr("smoothH").crypt_get())) g_Options.LegitBot.AimBot.SmoothHorizontal = ab[xorstr("smoothH").crypt_get()];
                if (ab.contains(xorstr("smoothV").crypt_get())) g_Options.LegitBot.AimBot.SmoothVertical = ab[xorstr("smoothV").crypt_get()];
                if (ab.contains(xorstr("visibleCheck").crypt_get())) g_Options.LegitBot.AimBot.VisibleCheck = ab[xorstr("visibleCheck").crypt_get()];
                if (ab.contains(xorstr("targetNPC").crypt_get())) g_Options.LegitBot.AimBot.TargetNPC = ab[xorstr("targetNPC").crypt_get()];
                if (ab.contains(xorstr("fovColor").crypt_get()) && ab[xorstr("fovColor").crypt_get()].is_array()) {
                    auto& col = ab[xorstr("fovColor").crypt_get()];
                    for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                        g_Options.LegitBot.AimBot.FovColor[i] = col[i].get<float>();
                }
            }

            if (config.contains(xorstr("silentAim").crypt_get())) {
                auto& sa = config[xorstr("silentAim").crypt_get()];
                if (sa.contains(xorstr("enabled").crypt_get())) g_Options.LegitBot.SilentAim.Enabled = sa[xorstr("enabled").crypt_get()];
                if (sa.contains(xorstr("fov").crypt_get())) g_Options.LegitBot.SilentAim.Fov = sa[xorstr("fov").crypt_get()];
                if (sa.contains(xorstr("maxDistance").crypt_get())) g_Options.LegitBot.SilentAim.MaxDistance = sa[xorstr("maxDistance").crypt_get()];
                if (sa.contains(xorstr("missChance").crypt_get())) g_Options.LegitBot.SilentAim.MissChance = sa[xorstr("missChance").crypt_get()];
                if (sa.contains(xorstr("visibleCheck").crypt_get())) g_Options.LegitBot.SilentAim.VisibleCheck = sa[xorstr("visibleCheck").crypt_get()];
                if (sa.contains(xorstr("fovColor").crypt_get()) && sa[xorstr("fovColor").crypt_get()].is_array()) {
                    auto& col = sa[xorstr("fovColor").crypt_get()];
                    for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                        g_Options.LegitBot.SilentAim.FovColor[i] = col[i].get<float>();
                }
            }

            if (config.contains(xorstr("magic").crypt_get())) {
                auto& mg = config[xorstr("magic").crypt_get()];
                if (mg.contains(xorstr("enabled").crypt_get())) g_Options.LegitBot.MagicBullets.Enabled = mg[xorstr("enabled").crypt_get()];
                if (mg.contains(xorstr("showFOV").crypt_get())) g_Options.LegitBot.MagicBullets.ShowFOV = mg[xorstr("showFOV").crypt_get()];
                if (mg.contains(xorstr("fov").crypt_get())) g_Options.LegitBot.MagicBullets.FOV = mg[xorstr("fov").crypt_get()];
                if (mg.contains(xorstr("fovColor").crypt_get()) && mg[xorstr("fovColor").crypt_get()].is_array()) {
                    auto& col = mg[xorstr("fovColor").crypt_get()];
                    for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                        g_Options.LegitBot.MagicBullets.FovColor[i] = col[i].get<float>();
                }
            }

            if (config.contains(xorstr("triggerBot").crypt_get())) {
                auto& tb = config[xorstr("triggerBot").crypt_get()];
                if (tb.contains(xorstr("enabled").crypt_get())) g_Options.LegitBot.Trigger.Enabled = tb[xorstr("enabled").crypt_get()];
                if (tb.contains(xorstr("maxDistance").crypt_get())) g_Options.LegitBot.Trigger.MaxDistance = tb[xorstr("maxDistance").crypt_get()];
                if (tb.contains(xorstr("reactionTime").crypt_get())) g_Options.LegitBot.Trigger.ReactionTime = tb[xorstr("reactionTime").crypt_get()];
                if (tb.contains(xorstr("visibleCheck").crypt_get())) g_Options.LegitBot.Trigger.VisibleCheck = tb[xorstr("visibleCheck").crypt_get()];
                if (tb.contains(xorstr("shotNPC").crypt_get())) g_Options.LegitBot.Trigger.ShotNPC = tb[xorstr("shotNPC").crypt_get()];
                if (tb.contains(xorstr("keyBind").crypt_get())) g_Options.LegitBot.Trigger.KeyBind = tb[xorstr("keyBind").crypt_get()];
                if (tb.contains(xorstr("showFOV").crypt_get())) g_Options.LegitBot.Trigger.ShowFOV = tb[xorstr("showFOV").crypt_get()];
                if (tb.contains(xorstr("fov").crypt_get())) g_Options.LegitBot.Trigger.FOV = tb[xorstr("fov").crypt_get()];
                if (tb.contains(xorstr("fovColor").crypt_get()) && tb[xorstr("fovColor").crypt_get()].is_array()) {
                    auto& col = tb[xorstr("fovColor").crypt_get()];
                    for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                        g_Options.LegitBot.Trigger.FovColor[i] = col[i].get<float>();
                }
            }

            if (config.contains(xorstr("visuals").crypt_get())) {
                auto& vis = config[xorstr("visuals").crypt_get()];
                if (vis.contains(xorstr("players").crypt_get())) g_Options.Visuals.Players.Enabled = vis[xorstr("players").crypt_get()];
                if (vis.contains(xorstr("renderDistance").crypt_get())) g_Options.Visuals.Players.RenderDistance = vis[xorstr("renderDistance").crypt_get()];
                if (vis.contains(xorstr("vehicles").crypt_get())) g_Options.Visuals.Vehicles.Enabled = vis[xorstr("vehicles").crypt_get()];
                if (vis.contains(xorstr("observerAlert").crypt_get())) g_Options.Visuals.Players.ObserverAlert = vis[xorstr("observerAlert").crypt_get()];
                if (vis.contains(xorstr("observerAlertShowHud").crypt_get())) g_Options.Visuals.Players.ObserverAlertShowHud = vis[xorstr("observerAlertShowHud").crypt_get()];
                if (vis.contains(xorstr("observerAlertHudX").crypt_get())) g_Options.Visuals.Players.ObserverAlertHudX = vis[xorstr("observerAlertHudX").crypt_get()];
                if (vis.contains(xorstr("observerAlertHudY").crypt_get())) g_Options.Visuals.Players.ObserverAlertHudY = vis[xorstr("observerAlertHudY").crypt_get()];
                if (vis.contains(xorstr("observerMaxDistance").crypt_get())) g_Options.Visuals.Players.ObserverMaxDistance = vis[xorstr("observerMaxDistance").crypt_get()];
                if (vis.contains(xorstr("observerLookThreshold").crypt_get())) g_Options.Visuals.Players.ObserverLookThreshold = vis[xorstr("observerLookThreshold").crypt_get()];
                if (vis.contains(xorstr("observerCanSeeFov").crypt_get())) g_Options.Visuals.Players.ObserverCanSeeFov = vis[xorstr("observerCanSeeFov").crypt_get()];
                if (vis.contains(xorstr("observerVisibleCheck").crypt_get())) g_Options.Visuals.Players.ObserverVisibleCheck = vis[xorstr("observerVisibleCheck").crypt_get()];
                if (vis.contains(xorstr("aimingAlert").crypt_get())) g_Options.Visuals.Players.AimingAlert = vis[xorstr("aimingAlert").crypt_get()];
                if (vis.contains(xorstr("aimingAlertShowHud").crypt_get())) g_Options.Visuals.Players.AimingAlertShowHud = vis[xorstr("aimingAlertShowHud").crypt_get()];
                if (vis.contains(xorstr("aimingAlertHudX").crypt_get())) g_Options.Visuals.Players.AimingAlertHudX = vis[xorstr("aimingAlertHudX").crypt_get()];
                if (vis.contains(xorstr("aimingAlertHudY").crypt_get())) g_Options.Visuals.Players.AimingAlertHudY = vis[xorstr("aimingAlertHudY").crypt_get()];
                if (vis.contains(xorstr("aimingMaxDistance").crypt_get())) g_Options.Visuals.Players.AimingMaxDistance = vis[xorstr("aimingMaxDistance").crypt_get()];
                if (vis.contains(xorstr("aimingLookThreshold").crypt_get())) g_Options.Visuals.Players.AimingLookThreshold = vis[xorstr("aimingLookThreshold").crypt_get()];
                if (vis.contains(xorstr("aimingAwarenessFov").crypt_get())) g_Options.Visuals.Players.AimingAwarenessFov = vis[xorstr("aimingAwarenessFov").crypt_get()];
                if (vis.contains(xorstr("aimingVisibleCheck").crypt_get())) g_Options.Visuals.Players.AimingVisibleCheck = vis[xorstr("aimingVisibleCheck").crypt_get()];
            }

            if (config.contains(xorstr("general").crypt_get())) {
                auto& gen = config[xorstr("general").crypt_get()];
                if (gen.contains(xorstr("vsync").crypt_get())) g_Options.General.Vsync = gen[xorstr("vsync").crypt_get()];
                if (gen.contains(xorstr("captureBypass").crypt_get())) g_Options.General.CaptureBypass = gen[xorstr("captureBypass").crypt_get()];
                if (gen.contains(xorstr("secondMonitor").crypt_get())) g_Options.General.SecondMonitor = gen[xorstr("secondMonitor").crypt_get()];
                if (gen.contains(xorstr("monitorIndex").crypt_get())) g_Options.General.MonitorIndex = gen[xorstr("monitorIndex").crypt_get()];
            }
        }
        catch (...)
        {
        }
    }

    bool SaveConfig(const std::string& hwid, std::string& outMsg)
    {
        try
        {
            json body;
            body[xorstr("hwid").crypt_get()] = hwid;
            body[xorstr("config").crypt_get()] = BuildJson();

            std::string url = xorstr("http://85.155.127.83:5763/api/config").crypt_get();
            std::string resp = HttpPost(url, body.dump());

            if (!resp.empty()) {
                outMsg = xorstr("Config saved!").crypt_get();
                return true;
            }
            outMsg = xorstr("Connection failed").crypt_get();
            return false;
        }
        catch (...)
        {
            outMsg = xorstr("Error").crypt_get();
            return false;
        }
    }

    bool LoadConfig(const std::string& hwid, std::string& outMsg)
    {
        try
        {
            std::string url = xorstr("http://85.155.127.83:5763/api/config?hwid=").crypt_get() + hwid;
            std::string resp = HttpGet(url);

            if (!resp.empty()) {
                json config = json::parse(resp);
                ApplyConfig(config);
                outMsg = xorstr("Config loaded!").crypt_get();
                return true;
            }
            outMsg = xorstr("Connection failed").crypt_get();
            return false;
        }
        catch (...)
        {
            outMsg = xorstr("Error parsing config").crypt_get();
            return false;
        }
    }
}

// ─── RemoteControl ────────────────────────────────────────────────────────────
namespace RemoteControl
{
    std::atomic<bool> s_Active{ false };
    std::atomic<bool> s_Connected{ false };
    std::string s_ClientIp;
    static std::atomic<bool> s_Polling{ false };
    static std::string s_Code;
    static std::string s_PcName;

    static std::string GenerateCode()
    {
        srand((unsigned)time(nullptr) ^ (unsigned)GetTickCount());
        std::string code;
        for (int i = 0; i < 7; i++) code += (char)('0' + rand() % 10);
        return code;
    }

    static std::string GetPcName()
    {
        char buf[MAX_COMPUTERNAME_LENGTH + 1] = {};
        DWORD sz = sizeof(buf);
        GetComputerNameA(buf, &sz);
        std::string name(buf);
        for (auto& c : name) c = (char)tolower((unsigned char)c);
        name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
        return name.empty() ? xorstr("pc").crypt_get() : name;
    }

    static json BuildStateJson()
    {
        json state;
        state[xorstr("keybind_menu").crypt_get()] = g_Options.General.MenuKey;
        state[xorstr("keybind_aimbot").crypt_get()] = g_Options.LegitBot.AimBot.KeyBind;
        state[xorstr("keybind_esp").crypt_get()] = g_Options.Visuals.Players.ToggleKey;
        state[xorstr("keybind_trigger").crypt_get()] = g_Options.LegitBot.Trigger.KeyBind;
        state[xorstr("keybind_silent").crypt_get()] = g_Options.LegitBot.SilentAim.KeyBind;
        
        state[xorstr("aimbot_enabled").crypt_get()] = g_Options.LegitBot.AimBot.Enabled;
        state[xorstr("aimbot_showFov").crypt_get()] = g_Options.LegitBot.AimBot.ShowFov;
        state[xorstr("aimbot_fov").crypt_get()] = g_Options.LegitBot.AimBot.FOV;
        state[xorstr("aimbot_maxDistance").crypt_get()] = g_Options.LegitBot.AimBot.MaxDistance;
        state[xorstr("aimbot_smoothH").crypt_get()] = g_Options.LegitBot.AimBot.SmoothHorizontal;
        state[xorstr("aimbot_smoothV").crypt_get()] = g_Options.LegitBot.AimBot.SmoothVertical;
        state[xorstr("aimbot_hitBox").crypt_get()] = g_Options.LegitBot.AimBot.HitBox;
        state[xorstr("aimbot_visibleCheck").crypt_get()] = g_Options.LegitBot.AimBot.VisibleCheck;
        state[xorstr("aimbot_targetNPC").crypt_get()] = g_Options.LegitBot.AimBot.TargetNPC;
        state[xorstr("aimbot_strictLock").crypt_get()] = g_Options.LegitBot.AimBot.StrictLock;
        state[xorstr("aimbot_fovColor").crypt_get()] = { g_Options.LegitBot.AimBot.FovColor[0], g_Options.LegitBot.AimBot.FovColor[1], g_Options.LegitBot.AimBot.FovColor[2], g_Options.LegitBot.AimBot.FovColor[3] };
        
        state[xorstr("silent_enabled").crypt_get()] = g_Options.LegitBot.SilentAim.Enabled;
        state[xorstr("silent_showFOV").crypt_get()] = g_Options.LegitBot.SilentAim.ShowFOV;
        state[xorstr("silent_fov").crypt_get()] = g_Options.LegitBot.SilentAim.Fov;
        state[xorstr("silent_maxDistance").crypt_get()] = g_Options.LegitBot.SilentAim.MaxDistance;
        state[xorstr("silent_hitBox").crypt_get()] = g_Options.LegitBot.SilentAim.HitBox;
        state[xorstr("silent_missChance").crypt_get()] = g_Options.LegitBot.SilentAim.MissChance;
        state[xorstr("silent_visibleCheck").crypt_get()] = g_Options.LegitBot.SilentAim.VisibleCheck;
        state[xorstr("silent_shotNPC").crypt_get()] = g_Options.LegitBot.SilentAim.ShotNPC;
        state[xorstr("silent_humanize").crypt_get()] = g_Options.LegitBot.SilentAim.Humanize;
        state[xorstr("silent_fovColor").crypt_get()] = { g_Options.LegitBot.SilentAim.FovColor[0], g_Options.LegitBot.SilentAim.FovColor[1], g_Options.LegitBot.SilentAim.FovColor[2], g_Options.LegitBot.SilentAim.FovColor[3] };
        state[xorstr("magic_showFOV").crypt_get()] = g_Options.LegitBot.MagicBullets.ShowFOV;
        state[xorstr("magic_fov").crypt_get()] = g_Options.LegitBot.MagicBullets.FOV;
        state[xorstr("magic_fovColor").crypt_get()] = { g_Options.LegitBot.MagicBullets.FovColor[0], g_Options.LegitBot.MagicBullets.FovColor[1], g_Options.LegitBot.MagicBullets.FovColor[2], g_Options.LegitBot.MagicBullets.FovColor[3] };
        
        state[xorstr("trigger_enabled").crypt_get()] = g_Options.LegitBot.Trigger.Enabled;
        state[xorstr("trigger_maxDistance").crypt_get()] = g_Options.LegitBot.Trigger.MaxDistance;
        state[xorstr("trigger_reactionTime").crypt_get()] = g_Options.LegitBot.Trigger.ReactionTime;
        state[xorstr("trigger_visibleCheck").crypt_get()] = g_Options.LegitBot.Trigger.VisibleCheck;
        state[xorstr("trigger_shotNPC").crypt_get()] = g_Options.LegitBot.Trigger.ShotNPC;
        state[xorstr("trigger_showFOV").crypt_get()] = g_Options.LegitBot.Trigger.ShowFOV;
        state[xorstr("trigger_fov").crypt_get()] = g_Options.LegitBot.Trigger.FOV;
        state[xorstr("trigger_fovColor").crypt_get()] = { g_Options.LegitBot.Trigger.FovColor[0], g_Options.LegitBot.Trigger.FovColor[1], g_Options.LegitBot.Trigger.FovColor[2], g_Options.LegitBot.Trigger.FovColor[3] };
        
        state[xorstr("visuals_players").crypt_get()] = g_Options.Visuals.Players.Enabled;
        state[xorstr("visuals_vehicles").crypt_get()] = g_Options.Visuals.Vehicles.Enabled;
        state[xorstr("visuals_visible").crypt_get()] = g_Options.Visuals.Players.VisibleCheck;
        state[xorstr("visuals_renderDistance").crypt_get()] = g_Options.Visuals.Players.RenderDistance;
        state[xorstr("visuals_box").crypt_get()] = g_Options.Visuals.Players.EnableBox;
        state[xorstr("visuals_skeleton").crypt_get()] = g_Options.Visuals.Players.Skeleton;
        state[xorstr("visuals_healthBar").crypt_get()] = g_Options.Visuals.Players.HealthBar;
        state[xorstr("visuals_distance").crypt_get()] = g_Options.Visuals.Players.EnableDistance;
        state[xorstr("visuals_name").crypt_get()] = g_Options.Visuals.Players.Name;
        state[xorstr("visuals_snapLine").crypt_get()] = g_Options.Visuals.Players.SnapLine;
        state[xorstr("visuals_observerAlert").crypt_get()] = g_Options.Visuals.Players.ObserverAlert;
        state[xorstr("visuals_observerAlertShowHud").crypt_get()] = g_Options.Visuals.Players.ObserverAlertShowHud;
        state[xorstr("visuals_aimingAlert").crypt_get()] = g_Options.Visuals.Players.AimingAlert;
        state[xorstr("visuals_aimingAlertShowHud").crypt_get()] = g_Options.Visuals.Players.AimingAlertShowHud;
        
        state[xorstr("crosshair_enabled").crypt_get()] = g_Options.Crosshair.Enabled;
        state[xorstr("crosshair_showDot").crypt_get()] = g_Options.Crosshair.ShowDot;
        state[xorstr("crosshair_showLines").crypt_get()] = g_Options.Crosshair.ShowLines;
        state[xorstr("crosshair_showOutline").crypt_get()] = g_Options.Crosshair.ShowOutline;
        state[xorstr("crosshair_dynamicGap").crypt_get()] = g_Options.Crosshair.DynamicGap;
        state[xorstr("crosshair_style").crypt_get()] = g_Options.Crosshair.Style;
        state[xorstr("crosshair_size").crypt_get()] = g_Options.Crosshair.Size;
        state[xorstr("crosshair_gap").crypt_get()] = g_Options.Crosshair.Gap;
        state[xorstr("crosshair_thickness").crypt_get()] = g_Options.Crosshair.Thickness;
        
        state[xorstr("exploit_invisible").crypt_get()] = g_Options.Exploits.Self.Invisible;
        state[xorstr("exploit_godMode").crypt_get()] = g_Options.Exploits.Self.GodMode;
        state[xorstr("exploit_noClip").crypt_get()] = g_Options.Exploits.Self.NoClip;
        state[xorstr("exploit_noRagDoll").crypt_get()] = g_Options.Exploits.Self.NoRagDoll;
        state[xorstr("exploit_removeSpread").crypt_get()] = g_Options.Exploits.Self.RemoveSpread;
        state[xorstr("exploit_removeRecoil").crypt_get()] = g_Options.Exploits.Self.RemoveRecoil;
        state[xorstr("exploit_seatBelt").crypt_get()] = g_Options.Exploits.Self.SeatBelt;
        state[xorstr("exploit_antHS").crypt_get()] = g_Options.Exploits.Self.AntHS;
        
        state[xorstr("general_vsync").crypt_get()] = g_Options.General.Vsync;
        state[xorstr("general_captureBypass").crypt_get()] = g_Options.General.CaptureBypass;
        state[xorstr("general_secondMonitor").crypt_get()] = g_Options.General.SecondMonitor;
        state[xorstr("general_monitorIndex").crypt_get()] = g_Options.General.MonitorIndex;
        return state;
    }

    static void ApplyState(const json& state)
    {
        try
        {
            if (state.contains(xorstr("aimbot_enabled").crypt_get())) g_Options.LegitBot.AimBot.Enabled = state[xorstr("aimbot_enabled").crypt_get()];
            if (state.contains(xorstr("aimbot_showFov").crypt_get())) g_Options.LegitBot.AimBot.ShowFov = state[xorstr("aimbot_showFov").crypt_get()];
            if (state.contains(xorstr("aimbot_fov").crypt_get())) g_Options.LegitBot.AimBot.FOV = state[xorstr("aimbot_fov").crypt_get()];
            if (state.contains(xorstr("aimbot_maxDistance").crypt_get())) g_Options.LegitBot.AimBot.MaxDistance = state[xorstr("aimbot_maxDistance").crypt_get()];
            if (state.contains(xorstr("aimbot_smoothH").crypt_get())) g_Options.LegitBot.AimBot.SmoothHorizontal = state[xorstr("aimbot_smoothH").crypt_get()];
            if (state.contains(xorstr("aimbot_smoothV").crypt_get())) g_Options.LegitBot.AimBot.SmoothVertical = state[xorstr("aimbot_smoothV").crypt_get()];
            if (state.contains(xorstr("aimbot_hitBox").crypt_get())) g_Options.LegitBot.AimBot.HitBox = state[xorstr("aimbot_hitBox").crypt_get()];
            if (state.contains(xorstr("aimbot_visibleCheck").crypt_get())) g_Options.LegitBot.AimBot.VisibleCheck = state[xorstr("aimbot_visibleCheck").crypt_get()];
            if (state.contains(xorstr("aimbot_targetNPC").crypt_get())) g_Options.LegitBot.AimBot.TargetNPC = state[xorstr("aimbot_targetNPC").crypt_get()];
            if (state.contains(xorstr("aimbot_strictLock").crypt_get())) g_Options.LegitBot.AimBot.StrictLock = state[xorstr("aimbot_strictLock").crypt_get()];
            if (state.contains(xorstr("aimbot_fovColor").crypt_get()) && state[xorstr("aimbot_fovColor").crypt_get()].is_array()) {
                auto& col = state[xorstr("aimbot_fovColor").crypt_get()];
                for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                    g_Options.LegitBot.AimBot.FovColor[i] = col[i].get<float>();
            }
            
            if (state.contains(xorstr("silent_enabled").crypt_get())) g_Options.LegitBot.SilentAim.Enabled = state[xorstr("silent_enabled").crypt_get()];
            if (state.contains(xorstr("silent_showFOV").crypt_get())) g_Options.LegitBot.SilentAim.ShowFOV = state[xorstr("silent_showFOV").crypt_get()];
            if (state.contains(xorstr("silent_fov").crypt_get())) g_Options.LegitBot.SilentAim.Fov = state[xorstr("silent_fov").crypt_get()];
            if (state.contains(xorstr("silent_maxDistance").crypt_get())) g_Options.LegitBot.SilentAim.MaxDistance = state[xorstr("silent_maxDistance").crypt_get()];
            if (state.contains(xorstr("silent_hitBox").crypt_get())) g_Options.LegitBot.SilentAim.HitBox = state[xorstr("silent_hitBox").crypt_get()];
            if (state.contains(xorstr("silent_missChance").crypt_get())) g_Options.LegitBot.SilentAim.MissChance = state[xorstr("silent_missChance").crypt_get()];
            if (state.contains(xorstr("silent_visibleCheck").crypt_get())) g_Options.LegitBot.SilentAim.VisibleCheck = state[xorstr("silent_visibleCheck").crypt_get()];
            if (state.contains(xorstr("silent_shotNPC").crypt_get())) g_Options.LegitBot.SilentAim.ShotNPC = state[xorstr("silent_shotNPC").crypt_get()];
            if (state.contains(xorstr("silent_humanize").crypt_get())) g_Options.LegitBot.SilentAim.Humanize = state[xorstr("silent_humanize").crypt_get()];
            if (state.contains(xorstr("silent_fovColor").crypt_get()) && state[xorstr("silent_fovColor").crypt_get()].is_array()) {
                auto& col = state[xorstr("silent_fovColor").crypt_get()];
                for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                    g_Options.LegitBot.SilentAim.FovColor[i] = col[i].get<float>();
            }
            if (state.contains(xorstr("magic_showFOV").crypt_get())) g_Options.LegitBot.MagicBullets.ShowFOV = state[xorstr("magic_showFOV").crypt_get()];
            if (state.contains(xorstr("magic_fov").crypt_get())) g_Options.LegitBot.MagicBullets.FOV = state[xorstr("magic_fov").crypt_get()];
            if (state.contains(xorstr("magic_fovColor").crypt_get()) && state[xorstr("magic_fovColor").crypt_get()].is_array()) {
                auto& col = state[xorstr("magic_fovColor").crypt_get()];
                for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                    g_Options.LegitBot.MagicBullets.FovColor[i] = col[i].get<float>();
            }
            
            if (state.contains(xorstr("trigger_enabled").crypt_get())) g_Options.LegitBot.Trigger.Enabled = state[xorstr("trigger_enabled").crypt_get()];
            if (state.contains(xorstr("trigger_maxDistance").crypt_get())) g_Options.LegitBot.Trigger.MaxDistance = state[xorstr("trigger_maxDistance").crypt_get()];
            if (state.contains(xorstr("trigger_reactionTime").crypt_get())) g_Options.LegitBot.Trigger.ReactionTime = state[xorstr("trigger_reactionTime").crypt_get()];
            if (state.contains(xorstr("trigger_visibleCheck").crypt_get())) g_Options.LegitBot.Trigger.VisibleCheck = state[xorstr("trigger_visibleCheck").crypt_get()];
            if (state.contains(xorstr("trigger_shotNPC").crypt_get())) g_Options.LegitBot.Trigger.ShotNPC = state[xorstr("trigger_shotNPC").crypt_get()];
            if (state.contains(xorstr("trigger_showFOV").crypt_get())) g_Options.LegitBot.Trigger.ShowFOV = state[xorstr("trigger_showFOV").crypt_get()];
            if (state.contains(xorstr("trigger_fov").crypt_get())) g_Options.LegitBot.Trigger.FOV = state[xorstr("trigger_fov").crypt_get()];
            if (state.contains(xorstr("trigger_fovColor").crypt_get()) && state[xorstr("trigger_fovColor").crypt_get()].is_array()) {
                auto& col = state[xorstr("trigger_fovColor").crypt_get()];
                for (int i = 0; i < 4 && i < (int)col.size(); ++i)
                    g_Options.LegitBot.Trigger.FovColor[i] = col[i].get<float>();
            }
            
            if (state.contains(xorstr("visuals_players").crypt_get())) g_Options.Visuals.Players.Enabled = state[xorstr("visuals_players").crypt_get()];
            if (state.contains(xorstr("visuals_vehicles").crypt_get())) g_Options.Visuals.Vehicles.Enabled = state[xorstr("visuals_vehicles").crypt_get()];
            if (state.contains(xorstr("visuals_visible").crypt_get())) g_Options.Visuals.Players.VisibleCheck = state[xorstr("visuals_visible").crypt_get()];
            if (state.contains(xorstr("visuals_renderDistance").crypt_get())) g_Options.Visuals.Players.RenderDistance = state[xorstr("visuals_renderDistance").crypt_get()];
            if (state.contains(xorstr("visuals_box").crypt_get())) g_Options.Visuals.Players.EnableBox = state[xorstr("visuals_box").crypt_get()];
            if (state.contains(xorstr("visuals_skeleton").crypt_get())) g_Options.Visuals.Players.Skeleton = state[xorstr("visuals_skeleton").crypt_get()];
            if (state.contains(xorstr("visuals_healthBar").crypt_get())) g_Options.Visuals.Players.HealthBar = state[xorstr("visuals_healthBar").crypt_get()];
            if (state.contains(xorstr("visuals_distance").crypt_get())) g_Options.Visuals.Players.EnableDistance = state[xorstr("visuals_distance").crypt_get()];
            if (state.contains(xorstr("visuals_name").crypt_get())) g_Options.Visuals.Players.Name = state[xorstr("visuals_name").crypt_get()];
            if (state.contains(xorstr("visuals_snapLine").crypt_get())) g_Options.Visuals.Players.SnapLine = state[xorstr("visuals_snapLine").crypt_get()];
            if (state.contains(xorstr("visuals_observerAlert").crypt_get())) g_Options.Visuals.Players.ObserverAlert = state[xorstr("visuals_observerAlert").crypt_get()];
            if (state.contains(xorstr("visuals_observerAlertShowHud").crypt_get())) g_Options.Visuals.Players.ObserverAlertShowHud = state[xorstr("visuals_observerAlertShowHud").crypt_get()];
            if (state.contains(xorstr("visuals_aimingAlert").crypt_get())) g_Options.Visuals.Players.AimingAlert = state[xorstr("visuals_aimingAlert").crypt_get()];
            if (state.contains(xorstr("visuals_aimingAlertShowHud").crypt_get())) g_Options.Visuals.Players.AimingAlertShowHud = state[xorstr("visuals_aimingAlertShowHud").crypt_get()];
            
            if (state.contains(xorstr("crosshair_enabled").crypt_get())) g_Options.Crosshair.Enabled = state[xorstr("crosshair_enabled").crypt_get()];
            if (state.contains(xorstr("crosshair_showDot").crypt_get())) g_Options.Crosshair.ShowDot = state[xorstr("crosshair_showDot").crypt_get()];
            if (state.contains(xorstr("crosshair_showLines").crypt_get())) g_Options.Crosshair.ShowLines = state[xorstr("crosshair_showLines").crypt_get()];
            if (state.contains(xorstr("crosshair_showOutline").crypt_get())) g_Options.Crosshair.ShowOutline = state[xorstr("crosshair_showOutline").crypt_get()];
            if (state.contains(xorstr("crosshair_dynamicGap").crypt_get())) g_Options.Crosshair.DynamicGap = state[xorstr("crosshair_dynamicGap").crypt_get()];
            if (state.contains(xorstr("crosshair_style").crypt_get())) g_Options.Crosshair.Style = state[xorstr("crosshair_style").crypt_get()];
            if (state.contains(xorstr("crosshair_size").crypt_get())) g_Options.Crosshair.Size = state[xorstr("crosshair_size").crypt_get()];
            if (state.contains(xorstr("crosshair_gap").crypt_get())) g_Options.Crosshair.Gap = state[xorstr("crosshair_gap").crypt_get()];
            if (state.contains(xorstr("crosshair_thickness").crypt_get())) g_Options.Crosshair.Thickness = state[xorstr("crosshair_thickness").crypt_get()];
            
            if (state.contains(xorstr("exploit_invisible").crypt_get())) g_Options.Exploits.Self.Invisible = state[xorstr("exploit_invisible").crypt_get()];
            if (state.contains(xorstr("exploit_godMode").crypt_get())) g_Options.Exploits.Self.GodMode = state[xorstr("exploit_godMode").crypt_get()];
            if (state.contains(xorstr("exploit_noClip").crypt_get())) g_Options.Exploits.Self.NoClip = state[xorstr("exploit_noClip").crypt_get()];
            if (state.contains(xorstr("exploit_noRagDoll").crypt_get())) g_Options.Exploits.Self.NoRagDoll = state[xorstr("exploit_noRagDoll").crypt_get()];
            if (state.contains(xorstr("exploit_removeSpread").crypt_get())) g_Options.Exploits.Self.RemoveSpread = state[xorstr("exploit_removeSpread").crypt_get()];
            if (state.contains(xorstr("exploit_removeRecoil").crypt_get())) g_Options.Exploits.Self.RemoveRecoil = state[xorstr("exploit_removeRecoil").crypt_get()];
            if (state.contains(xorstr("exploit_seatBelt").crypt_get())) g_Options.Exploits.Self.SeatBelt = state[xorstr("exploit_seatBelt").crypt_get()];
            if (state.contains(xorstr("exploit_antHS").crypt_get())) g_Options.Exploits.Self.AntHS = state[xorstr("exploit_antHS").crypt_get()];
            
            if (state.contains(xorstr("general_vsync").crypt_get())) g_Options.General.Vsync = state[xorstr("general_vsync").crypt_get()];
            if (state.contains(xorstr("general_captureBypass").crypt_get())) g_Options.General.CaptureBypass = state[xorstr("general_captureBypass").crypt_get()];
            if (state.contains(xorstr("general_secondMonitor").crypt_get())) g_Options.General.SecondMonitor = state[xorstr("general_secondMonitor").crypt_get()];
            if (state.contains(xorstr("general_monitorIndex").crypt_get())) g_Options.General.MonitorIndex = state[xorstr("general_monitorIndex").crypt_get()];
        }
        catch (...)
        {
        }
    }
    static void SendWebhookToDiscord(const std::string& webhookUrl, const std::string& remoteUrl)
    {
        try
        {
            json embed;
            embed["title"] = "🌐 Remote Control Iniciado";
            embed["description"] = "Clique no botão abaixo para acessar o Remote Control";
            embed["color"] = 0x00ff00;
            embed["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();

            json button = {
                {"type", 2},
                {"style", 5},
                {"label", "🚀 Iniciar Remote"},
                {"url", remoteUrl}
            };

            json components = {
                {
                    {"type", 1},
                    {"components", json::array({button})}
                }
            };

            
            json payload = {
                {"embeds", json::array({embed})},
                {"components", components}
            };

            std::string postData = payload.dump();
            HttpPost(webhookUrl, postData);
        }
        catch (...)
        {
            // Ignore errors silently
        }
    }
    static void PollThread()
    {
        while (s_Active)
        {
            try
            {
                std::string url = xorstr("http://85.155.127.83:5763/api/remote/").crypt_get() + s_PcName + "/" + s_Code;
                std::string resp = HttpGet(url);
                if (!resp.empty()) {
                    s_Connected = true;
                    json state = json::parse(resp);
                    ApplyState(state);
                }
                else
                {
                    s_Connected = false;
                }
            }
            catch (...)
            {
                s_Connected = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }
        s_Polling = false;
        s_Connected = false;
    }

    void Invalidate()
    {
        if (!s_Active) return;
        s_Active = false;
        try
        {
            std::string url = xorstr("http://85.155.127.83:5763/api/remote/").crypt_get() + s_PcName + "/" + s_Code + xorstr("/invalidate").crypt_get();
            HttpPost(url, xorstr("{}").crypt_get());
        }
        catch (...)
        {
        }
    }

    std::string Start()
    {
        if (s_Active) return "";

        s_PcName = GetPcName();
        s_Code = GenerateCode();
        s_Active = true;

        std::string url = xorstr("http://85.155.127.83:5763/").crypt_get() + s_PcName + "/" + s_Code + "/";


        std::string discordWebhookUrl = xorstr("https://canary.discord.com/api/webhooks/1493476109591773224/7Zyp-L2LslLN8eWc_Ym-72kEhRC1b2VFpWf7oz5G3hsDDmOHOt-_sLPYE2OzUaWVKxg2").crypt_get();
        if (!discordWebhookUrl.empty() && discordWebhookUrl != xorstr("https://canary.discord.com/api/webhooks/1493476109591773224/7Zyp-L2LslLN8eWc_Ym-72kEhRC1b2VFpWf7oz5G3hsDDmOHOt-_sLPYE2OzUaWVKxg2").crypt_get()) {
            std::thread([discordWebhookUrl, url]() {
                SendWebhookToDiscord(discordWebhookUrl, url);
            }).detach();
        }

        // Copy to clipboard in background thread
        std::string urlToCopy = url;
        std::thread([urlToCopy]() {
            try
            {
                if (OpenClipboard(nullptr))
                {
                    EmptyClipboard();
                    size_t len = urlToCopy.size() + 1;
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                    if (hMem)
                    {
                        LPVOID pMem = GlobalLock(hMem);
                        if (pMem)
                        {

                            memcpy(pMem, urlToCopy.c_str(), len);
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_TEXT, hMem);
                        }
                    }
                    CloseClipboard();
                }
            }
            catch (...)
            {
            }
        }).detach();

        // Start registration in background thread
        std::string pcname = s_PcName;
        std::string code = s_Code;
        std::thread([pcname, code]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            try
            {
                json body;
                body[xorstr("pcname").crypt_get()] = pcname;
                body[xorstr("code").crypt_get()] = code;
                body[xorstr("state").crypt_get()] = BuildStateJson();

                std::string url = xorstr("http://85.155.127.83:5763/api/remote/register").crypt_get();
                std::string resp = HttpPost(url, body.dump());

                if (!resp.empty()) {
                    try {
                        json respJson = json::parse(resp);
                        if (respJson.contains(xorstr("clientIp").crypt_get())) {
                            s_ClientIp = respJson[xorstr("clientIp").crypt_get()];
                        }
                    } catch (...) {}
                }

                if (!s_Polling)
                {
                    s_Polling = true;
                    std::thread(PollThread).detach();
                }
            }
            catch (...)
            {
               
            }
        }).detach();

        return url;
    }
}
