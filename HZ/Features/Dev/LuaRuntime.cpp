#include "LuaRuntime.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "DevBridge.hpp"
#include "ServerDump.hpp"
#include "../../Utils/Memory.hpp"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>

namespace Cheat
{
	namespace LuaRuntime
	{
		namespace
		{
			std::mutex g_mutex;
			std::atomic<bool> g_running{ false };
			std::atomic<bool> g_harvesting{ false };
			StateUpdateFn g_onUpdated;

			Status g_status;
			std::vector<std::string> g_harvestedResources;
			std::vector<DevTriggerEntry> g_triggers;

			std::string g_lastSessionKey;
			bool g_wasAttached = false;
			uint64_t g_attachTick = 0;
			bool g_postAttachHarvestScheduled = false;

			std::string BuildSessionKey(const ServerDump::ServerSession& session)
			{
				return session.label + "|" + session.infoPath.string() + "|" + std::to_string(session.lastWriteUnix);
			}

			void SetMessage(const std::string& message)
			{
				std::lock_guard<std::mutex> lock(g_mutex);
				g_status.message = message;
			}

			void RunHarvestCycle()
			{
				if (g_harvesting.exchange(true))
					return;

				SetMessage("Reading server memory for client scripts...");

				const ServerDump::ServerScanResult scan = ServerDump::ScanActiveServer();
				const ServerDump::RuntimeHarvestResult harvest = ServerDump::HarvestRuntimeClientScripts();

				if (g_onUpdated)
				{
					g_onUpdated(
						scan.session.label,
						scan.session.cachePath.string(),
						harvest.clientFiles,
						scan.resources,
						scan.triggers);
				}

				{
					std::lock_guard<std::mutex> lock(g_mutex);
					g_status.harvesting = false;
					g_status.ready = harvest.success;
					g_status.serverLabel = scan.session.label;
					g_status.harvestPath = harvest.outputPath.string();
					g_status.clientScripts = harvest.clientFiles;
					g_status.resourceCount = harvest.resourceCount;
					g_status.message = harvest.message;
					g_harvestedResources = harvest.resourceNames;
					g_triggers = scan.triggers;
				}

				g_harvesting.store(false);
			}

			std::string ExtractEventFromPayload(const std::string& payload)
			{
				const size_t pos = payload.find("TriggerServerEvent");
				if (pos == std::string::npos)
					return {};

				const size_t openQuote = payload.find('\'', pos);
				if (openQuote == std::string::npos)
					return {};

				const size_t closeQuote = payload.find('\'', openQuote + 1);
				if (closeQuote == std::string::npos)
					return {};

				return payload.substr(openQuote + 1, closeQuote - openQuote - 1);
			}

			std::string NormalizeResourceName(const std::string& name)
			{
				std::string normalized = name;
				if (normalized.size() >= 2 &&
					((normalized.front() == '\'' && normalized.back() == '\'') ||
						(normalized.front() == '"' && normalized.back() == '"')))
				{
					normalized = normalized.substr(1, normalized.size() - 2);
				}

				std::transform(normalized.begin(), normalized.end(), normalized.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return normalized;
			}
		}

		void Start(StateUpdateFn onUpdated)
		{
			g_onUpdated = std::move(onUpdated);
			if (g_running.exchange(true))
				return;

			std::thread([]()
			{
				while (g_running.load())
				{
					const bool attached = FrameWork::Memory::AttachedProcessHandle != nullptr;
					const uint64_t nowTick = GetTickCount64();

					if (!attached)
					{
						g_wasAttached = false;
						g_postAttachHarvestScheduled = false;
						g_attachTick = 0;
						std::this_thread::sleep_for(std::chrono::seconds(2));
						continue;
					}

					if (!g_wasAttached)
					{
						g_wasAttached = true;
						g_attachTick = nowTick;
						g_postAttachHarvestScheduled = false;
					}

					const ServerDump::ServerSession session = ServerDump::GetActiveServerSession();
					const std::string sessionKey = BuildSessionKey(session);
					const bool sessionChanged = !sessionKey.empty() && sessionKey != g_lastSessionKey;
					const bool postAttachReady = !g_postAttachHarvestScheduled && g_attachTick != 0 &&
						(nowTick - g_attachTick) >= 6000;

					if (sessionChanged || postAttachReady)
					{
						g_lastSessionKey = sessionKey;
						if (postAttachReady)
							g_postAttachHarvestScheduled = true;
						std::thread(RunHarvestCycle).detach();
					}

					std::this_thread::sleep_for(std::chrono::seconds(2));
				}
			}).detach();
		}

		void Stop()
		{
			g_running.store(false);
		}

		Status GetStatus()
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			Status copy = g_status;
			copy.harvesting = g_harvesting.load();
			return copy;
		}

		std::string ResolveInjectResource(const std::string& requested, const std::string& payload)
		{
			std::lock_guard<std::mutex> lock(g_mutex);

			const std::string normalized = NormalizeResourceName(requested);
			if (!normalized.empty() && normalized != "any")
			{
				for (const auto& resource : g_harvestedResources)
				{
					if (NormalizeResourceName(resource) == normalized)
						return resource;
				}
				return requested;
			}

			const std::string eventName = ExtractEventFromPayload(payload);
			if (!eventName.empty())
			{
				for (const auto& trigger : g_triggers)
				{
					if (trigger.code.find(eventName) != std::string::npos)
						return trigger.resource;
				}

				const auto colon = eventName.find(':');
				if (colon != std::string::npos)
				{
					const std::string prefix = eventName.substr(0, colon);
					for (const auto& resource : g_harvestedResources)
					{
						if (resource.find(prefix) != std::string::npos)
							return resource;
					}
				}
			}

			for (const auto& trigger : g_triggers)
			{
				if (trigger.code.find("TriggerServerEvent") != std::string::npos)
					return trigger.resource;
			}

			if (!g_harvestedResources.empty())
				return g_harvestedResources.front();

			return "any";
		}

		std::string WrapForInjection(const std::string&, const std::string& payload)
		{
			return payload;
		}
	}
}

#else

namespace Cheat
{
	namespace LuaRuntime
	{
		void Start(StateUpdateFn) {}
		void Stop() {}
		Status GetStatus() { return {}; }
		std::string ResolveInjectResource(const std::string& requested, const std::string&) { return requested; }
		std::string WrapForInjection(const std::string&, const std::string& payload) { return payload; }
	}
}

#endif
