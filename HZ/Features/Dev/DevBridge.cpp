#include "DevBridge.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "DevBridgeHttp.hpp"
#include "LuaExecutor.hpp"
#include "LuaRuntime.hpp"
#include "ServerDump.hpp"
#include "../../Utils/BrandPaths.hpp"
#include "../../Utils/Memory.hpp"
#include "../../Render/CustomWidgets/Notify.hpp"
#include "../../json.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <thread>

namespace Cheat
{
	namespace DevBridge
	{
		namespace
		{
			constexpr size_t kMaxEvents = 500;

			std::mutex g_stateMutex;
			DevBridgeState g_state;
			std::string g_statusMessage = "Join a server — dump uses local cache and live FiveM memory (no server resource).";
			std::set<std::string> g_blockedEventSet;
			std::map<std::string, int> g_blockCounts;

			std::string NowTimestamp()
			{
				const auto now = std::chrono::system_clock::now();
				const std::time_t t = std::chrono::system_clock::to_time_t(now);
				std::tm localTime{};
				localtime_s(&localTime, &t);
				std::ostringstream ss;
				ss << std::put_time(&localTime, "%H:%M:%S");
				return ss.str();
			}

			void SetStatus(const std::string& message)
			{
				g_statusMessage = message;
			}

			bool CopyTextToClipboard(const std::string& text)
			{
				if (text.empty() || !OpenClipboard(nullptr))
					return false;

				EmptyClipboard();
				const size_t size = text.size() + 1;
				HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
				if (!memory)
				{
					CloseClipboard();
					return false;
				}

				void* locked = GlobalLock(memory);
				if (!locked)
				{
					GlobalFree(memory);
					CloseClipboard();
					return false;
				}

				memcpy(locked, text.c_str(), size);
				GlobalUnlock(memory);
				SetClipboardData(CF_TEXT, memory);
				CloseClipboard();
				return true;
			}

			void PushLocalEvent(std::vector<DevEventEntry>& list, const std::string& eventName, const std::string& data)
			{
				DevEventEntry entry;
				entry.timestamp = NowTimestamp();
				entry.event = eventName;
				entry.data = data;
				list.insert(list.begin(), std::move(entry));
				if (list.size() > kMaxEvents)
					list.resize(kMaxEvents);
			}

			std::string TrimLine(const std::string& line)
			{
				size_t start = 0;
				while (start < line.size() && std::isspace(static_cast<unsigned char>(line[start])))
					++start;

				size_t end = line.size();
				while (end > start && std::isspace(static_cast<unsigned char>(line[end - 1])))
					--end;

				return line.substr(start, end - start);
			}

			void SyncBlockedEventsLocked()
			{
				g_state.blockedEvents.clear();
				for (const auto& blocked : g_blockedEventSet)
					g_state.blockedEvents.push_back(blocked);
			}

			void SaveRunScript(const std::string& code)
			{
				const std::string luaDir = BrandPaths::GetDataRoot() + "lua";
				std::error_code ec;
				std::filesystem::create_directories(luaDir, ec);

				const std::string path = luaDir + "\\_run.lua";
				std::ofstream file(path, std::ios::binary | std::ios::trunc);
				if (file)
					file << code;
			}
		}

		void Start()
		{
			DevBridgeHttp::Start();
			LuaRuntime::Start([](const std::string& serverLabel,
				const std::string& cachePath,
				size_t scriptFiles,
				const std::vector<DevResourceEntry>& resources,
				const std::vector<DevTriggerEntry>& triggers)
			{
				std::lock_guard<std::mutex> lock(g_stateMutex);
				g_state.serverLabel = serverLabel;
				g_state.serverCachePath = cachePath;
				g_state.serverScriptFiles = scriptFiles;
				g_state.resources = resources;
				g_state.triggers = triggers;
				SetStatus("Runtime harvest complete — client scripts loaded for TriggerServerEvent.");
			});

			std::lock_guard<std::mutex> lock(g_stateMutex);
			if (DevBridgeHttp::IsBridgeLive())
			{
				g_state.connected = true;
				SetStatus("Bridge connected for optional in-game Lua execution.");
			}
			else
			{
				g_state.connected = false;
				SetStatus("Ready — join a server. Dump works from cache + memory without any server resource.");
			}
		}

		void Stop()
		{
			LuaRuntime::Stop();
			DevBridgeHttp::Stop();
		}

		bool IsRunning() { return true; }

		bool IsConnected() { return DevBridgeHttp::IsBridgeLive(); }

		bool IsFiveMAttached()
		{
			return FrameWork::Memory::AttachedProcessHandle != nullptr;
		}

		DevBridgeState GetState()
		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			DevBridgeState copy = g_state;
			copy.connected = DevBridgeHttp::IsBridgeLive();
			if (copy.connected)
				DevBridgeHttp::MergePushState(DevBridgeHttp::GetLastPushBody(), copy);
			return copy;
		}

		std::string GetStatusMessage()
		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			return g_statusMessage;
		}

		void SetSetting(const std::string& key, bool value)
		{
			{
				std::lock_guard<std::mutex> lock(g_stateMutex);
				if (key == "logCts") g_state.logCts = value;
				else if (key == "logStc") g_state.logStc = value;
				else if (key == "logNui") g_state.logNui = value;
			}

			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = nlohmann::json::object();
				if (key == "logCts") payload["logCts"] = value;
				else if (key == "logStc") payload["logStc"] = value;
				else if (key == "logNui") payload["logNui"] = value;
				DevBridgeHttp::QueueCommand("set_setting", payload.dump());
			}
		}

		void ExecuteLua(const std::string& code)
		{
			SaveRunScript(code);
			SetStatus("Executing Lua in FiveM...");
			NotifyManager::Send("Executing Lua...", 2000);

			std::thread([code]()
			{
				const bool ok = IsFiveMAttached() && LuaExecutor::ExecuteScript(code);
				if (ok)
				{
					const auto runtime = LuaRuntime::GetStatus();
					SetStatus("Executed in resource Lua state (" + runtime.serverLabel + ").");
					NotifyManager::Send("Lua executed in FiveM resource VM.", 3500);
					return;
				}

				{
					std::lock_guard<std::mutex> lock(g_stateMutex);
					if (code.find("TriggerServerEvent") != std::string::npos && g_state.logCts)
						PushLocalEvent(g_state.ctsEvents, "TriggerServerEvent", code);
					else if (code.find("TriggerEvent") != std::string::npos && g_state.logStc)
						PushLocalEvent(g_state.stcEvents, "TriggerEvent", code);

					DevThreadEntry thread;
					thread.name = "_run.lua";
					thread.status = "saved";
					g_state.threads.insert(g_state.threads.begin(), thread);
					if (g_state.threads.size() > 64)
						g_state.threads.resize(64);
				}

				const std::string error = LuaExecutor::LastError();
				SetStatus("Execute failed — " + (error.empty()
					? std::string("no resource Lua state found. Stay in-game until scripts finish loading.")
					: error));
				NotifyManager::Send("Lua execute failed (external).", 3500);
			}).detach();
		}

		void LoadLuaFile(const std::string& path)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"path", path} };
				DevBridgeHttp::QueueCommand("load_lua_file", payload.dump());
				SetStatus("Sent load request to in-game bridge.");
				return;
			}

			std::ifstream file(path, std::ios::binary);
			if (!file)
			{
				SetStatus("Could not open lua file.");
				return;
			}

			std::ostringstream ss;
			ss << file.rdbuf();
			ExecuteLua(ss.str());
		}

		void ScanTriggers()
		{
			if (DevBridgeHttp::IsBridgeLive())
				DevBridgeHttp::QueueCommand("scan_triggers");
			ScanServer();
		}

		void ResetLuaEnvironment()
		{
			if (DevBridgeHttp::IsBridgeLive())
				DevBridgeHttp::QueueCommand("reset_lua");

			std::lock_guard<std::mutex> lock(g_stateMutex);
			g_state.threads.clear();
			g_blockedEventSet.clear();
			g_blockCounts.clear();
			SyncBlockedEventsLocked();
			SetStatus("Cleared dev state.");
		}

		void DumpScripts(const std::string& directory)
		{
			DumpActiveServer(directory, false, true, false);
		}

		void DumpActiveServer(const std::string& directory, bool includeStreamables, bool includeScripts, bool includeAllFiles)
		{
			const std::filesystem::path outputRoot = directory.empty()
				? std::filesystem::path(BrandPaths::GetDataRoot()) / "dump"
				: std::filesystem::path(directory);

			std::error_code ec;
			std::filesystem::create_directories(outputRoot, ec);

			SetStatus("Dump started... this can take a while for large caches.");
			NotifyManager::Send("Dump started. Files will appear in: " + outputRoot.string(), 5000);

			std::thread([outputRoot, includeStreamables, includeScripts, includeAllFiles]()
			{
				const ServerDump::ServerDumpResult dump = ServerDump::DumpActiveServer(outputRoot, includeStreamables, includeScripts, includeAllFiles);
				SetStatus(dump.message);
				if (dump.success)
				{
					NotifyManager::Send(dump.message + " -> " + dump.outputPath.string(), 8000);
				}
				else
				{
					NotifyManager::Send(dump.message, 6000);
				}
			}).detach();
		}

		void ScanServer()
		{
			std::thread([]()
			{
				{
					std::lock_guard<std::mutex> lock(g_stateMutex);
					SetStatus("Scanning server cache...");
				}

				const ServerDump::ServerScanResult scan = ServerDump::ScanActiveServer();

				std::string status;
				{
					std::lock_guard<std::mutex> lock(g_stateMutex);
					g_state.serverLabel = scan.session.label;
					g_state.serverCachePath = scan.session.cachePath.string();
					g_state.serverStreamFiles = scan.streamFileCount;
					g_state.serverScriptFiles = scan.scriptFileCount;
					g_state.resources = scan.resources;
					g_state.triggers = scan.triggers;

					if (scan.session.cachePath.empty())
					{
						status = "No server cache found. Join a FiveM server, wait for resources to load, then Scan Server.";
					}
					else
					{
						std::ostringstream ss;
						ss << "Server: " << scan.session.label
							<< " | resources " << scan.resources.size()
							<< " | triggers " << scan.triggers.size()
							<< " | cache blobs " << scan.streamFileCount
							<< " | scripts " << scan.scriptFileCount;
						status = ss.str();
					}
					SetStatus(status);
				}

				NotifyManager::Send(status, 4500);
			}).detach();
		}

		void BlockEvent(const std::string& eventName)
		{
			if (eventName.empty())
				return;

			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"event", eventName} };
				DevBridgeHttp::QueueCommand("block_event", payload.dump());
			}

			std::lock_guard<std::mutex> lock(g_stateMutex);
			g_blockedEventSet.insert(eventName);
			g_blockCounts[eventName]++;
			SyncBlockedEventsLocked();
			SetStatus("Blocked event: " + eventName);
		}

		void ResendCtsEvent(size_t index)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"index", static_cast<int>(index)} };
				DevBridgeHttp::QueueCommand("resend_cts", payload.dump());
				SetStatus("Resent CTS event via bridge.");
				return;
			}

			std::string line;
			{
				std::lock_guard<std::mutex> lock(g_stateMutex);
				if (index >= g_state.ctsEvents.size())
					return;
				line = g_state.ctsEvents[index].event + " " + g_state.ctsEvents[index].data;
			}

			CopyTextToClipboard(TrimLine(line));
			SetStatus("Copied CTS event to clipboard.");
		}

		void ResendStcEvent(size_t index)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"index", static_cast<int>(index)} };
				DevBridgeHttp::QueueCommand("resend_stc", payload.dump());
				SetStatus("Resent STC event via bridge.");
				return;
			}

			std::string line;
			{
				std::lock_guard<std::mutex> lock(g_stateMutex);
				if (index >= g_state.stcEvents.size())
					return;
				line = g_state.stcEvents[index].event + " " + g_state.stcEvents[index].data;
			}

			CopyTextToClipboard(TrimLine(line));
			SetStatus("Copied STC event to clipboard.");
		}

		void StartResource(const std::string& name)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"name", name} };
				DevBridgeHttp::QueueCommand("resource_start", payload.dump());
				SetStatus("Started resource via bridge: " + name);
				return;
			}

			const std::string command = "ensure " + name;
			CopyTextToClipboard(command);
			SetStatus("Copied to clipboard: " + command);
		}

		void StopResource(const std::string& name)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"name", name} };
				DevBridgeHttp::QueueCommand("resource_stop", payload.dump());
				SetStatus("Stopped resource via bridge: " + name);
				return;
			}

			const std::string command = "stop " + name;
			CopyTextToClipboard(command);
			SetStatus("Copied to clipboard: " + command);
		}

		void TestNuiInject()
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				DevBridgeHttp::QueueCommand("test_nui_inject");
				SetStatus("Sent NUI test via bridge.");
				return;
			}

			const std::string snippet = "SendNUIMessage({ trinityDevTest = true })";
			CopyTextToClipboard(snippet);
			SetStatus("Copied NUI test snippet to clipboard.");
		}

		void ClearEvents(const std::string& channel)
		{
			if (DevBridgeHttp::IsBridgeLive())
			{
				nlohmann::json payload = { {"channel", channel} };
				DevBridgeHttp::QueueCommand("clear_events", payload.dump());
			}

			std::lock_guard<std::mutex> lock(g_stateMutex);
			if (channel == "cts") g_state.ctsEvents.clear();
			else if (channel == "stc") g_state.stcEvents.clear();
			else if (channel == "nui") g_state.nuiEvents.clear();
		}

		void LogManualEvent(const std::string& channel, const std::string& eventName, const std::string& data)
		{
			std::lock_guard<std::mutex> lock(g_stateMutex);
			if (channel == "cts" && g_state.logCts)
				PushLocalEvent(g_state.ctsEvents, eventName, data);
			else if (channel == "stc" && g_state.logStc)
				PushLocalEvent(g_state.stcEvents, eventName, data);
			else if (channel == "nui" && g_state.logNui)
				PushLocalEvent(g_state.nuiEvents, eventName, data);
		}
	}
}

#else

namespace Cheat
{
	namespace DevBridge
	{
		void Start() {}
		void Stop() {}
		bool IsRunning() { return false; }
		bool IsConnected() { return false; }
		bool IsFiveMAttached() { return false; }
		DevBridgeState GetState() { return {}; }
		std::string GetStatusMessage() { return {}; }
		void SetSetting(const std::string&, bool) {}
		void ExecuteLua(const std::string&) {}
		void LoadLuaFile(const std::string&) {}
		void ScanTriggers() {}
		void ResetLuaEnvironment() {}
		void DumpScripts(const std::string&) {}
		void DumpActiveServer(const std::string&, bool, bool, bool) {}
		void ScanServer() {}
		void BlockEvent(const std::string&) {}
		void ResendCtsEvent(size_t) {}
		void ResendStcEvent(size_t) {}
		void StartResource(const std::string&) {}
		void StopResource(const std::string&) {}
		void TestNuiInject() {}
		void ClearEvents(const std::string&) {}
		void LogManualEvent(const std::string&, const std::string&, const std::string&) {}
	}
}

#endif
