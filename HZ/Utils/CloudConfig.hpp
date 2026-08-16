#pragma once
#include <string>
#include <windows.h>
#include <wininet.h>
#include <sstream>
#include <atomic>

#pragma comment(lib, "wininet.lib")

namespace CloudConfig
{
    bool SaveConfig(const std::string& hwid, std::string& outMsg);
    bool LoadConfig(const std::string& hwid, std::string& outMsg);
}

namespace RemoteControl
{
    extern std::atomic<bool> s_Active;
    extern std::atomic<bool> s_Connected;
    extern std::string s_ClientIp;
    std::string Start();
    void Invalidate();
}
    