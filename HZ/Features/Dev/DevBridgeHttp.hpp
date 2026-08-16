#pragma once

#include "DevBridge.hpp"

#include <cstddef>
#include <filesystem>
#include <string>

namespace Cheat
{
	namespace DevBridgeHttp
	{
		void Start();
		void Stop();
		bool IsBridgeLive();
		std::string GetLastPushBody();

		void QueueCommand(const std::string& type, const std::string& payloadJson = "{}");
		void BeginDumpWait(const std::filesystem::path& outputRoot);
		bool WaitForDumpCompletion(unsigned timeoutMs, size_t& filesWritten);
		bool IsDumpPending();
		bool IsDumpDone();
		size_t GetDumpFilesWritten();

		void MergePushState(const std::string& jsonBody, DevBridgeState& state);
	}
}
