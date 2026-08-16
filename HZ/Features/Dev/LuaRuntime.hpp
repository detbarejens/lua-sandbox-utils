#pragma once

#include "DevBridge.hpp"

#include <functional>
#include <string>
#include <vector>

namespace Cheat
{
	namespace LuaRuntime
	{
		struct Status
		{
			bool harvesting = false;
			bool ready = false;
			std::string serverLabel;
			std::string harvestPath;
			size_t clientScripts = 0;
			size_t resourceCount = 0;
			std::string message;
		};

		using StateUpdateFn = std::function<void(
			const std::string& serverLabel,
			const std::string& cachePath,
			size_t scriptFiles,
			const std::vector<DevResourceEntry>& resources,
			const std::vector<DevTriggerEntry>& triggers)>;

		void Start(StateUpdateFn onUpdated);
		void Stop();

		Status GetStatus();
		std::string ResolveInjectResource(const std::string& requested, const std::string& payload);
		std::string WrapForInjection(const std::string& resource, const std::string& payload);
	}
}
