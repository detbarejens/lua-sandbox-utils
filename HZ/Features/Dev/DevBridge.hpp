#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Cheat
{
	struct DevEventEntry
	{
		std::string timestamp;
		std::string event;
		std::string data;
	};

	struct DevResourceEntry
	{
		std::string name;
		std::string state;
		int threadCount = 0;
		int eventCount = 0;
	};

	struct DevTriggerEntry
	{
		std::string resource;
		std::string code;
	};

	struct DevThreadEntry
	{
		std::string name;
		std::string status;
	};

	struct DevBridgeState
	{
		bool connected = false;
		bool logCts = false;
		bool logStc = false;
		bool logNui = false;
		std::string serverLabel;
		std::string serverCachePath;
		size_t serverStreamFiles = 0;
		size_t serverScriptFiles = 0;
		std::vector<DevResourceEntry> resources;
		std::vector<DevEventEntry> ctsEvents;
		std::vector<DevEventEntry> stcEvents;
		std::vector<DevEventEntry> nuiEvents;
		std::vector<DevTriggerEntry> triggers;
		std::vector<DevThreadEntry> threads;
		std::vector<std::string> blockedEvents;
	};

	namespace DevBridge
	{
		void Start();
		void Stop();
		bool IsRunning();
		bool IsConnected();
		bool IsFiveMAttached();

		DevBridgeState GetState();
		void SetSetting(const std::string& key, bool value);

		void ExecuteLua(const std::string& code);
		void LoadLuaFile(const std::string& path);
		void ScanTriggers();
		void ResetLuaEnvironment();
		void DumpScripts(const std::string& directory);
		void DumpActiveServer(const std::string& directory, bool includeStreamables, bool includeScripts, bool includeAllFiles);
		void ScanServer();
		void BlockEvent(const std::string& eventName);
		void ResendCtsEvent(size_t index);
		void ResendStcEvent(size_t index);
		void StartResource(const std::string& name);
		void StopResource(const std::string& name);
		void TestNuiInject();
		void ClearEvents(const std::string& channel);
		std::string GetStatusMessage();
		void LogManualEvent(const std::string& channel, const std::string& eventName, const std::string& data = {});
	}
}
