#pragma once



#include "DevBridge.hpp"



#include <filesystem>

#include <string>

#include <vector>



namespace Cheat

{

	namespace ServerDump

	{

		struct ServerSession

		{

			std::string label;

			std::filesystem::path cachePath;

			std::filesystem::path infoPath;

			int64_t lastWriteUnix = 0;

		};



		struct ServerScanResult

		{

			ServerSession session;

			std::vector<DevResourceEntry> resources;

			std::vector<DevTriggerEntry> triggers;

			size_t scriptFileCount = 0;

			size_t streamFileCount = 0;

			size_t totalFileCount = 0;

		};



		struct ServerDumpResult

		{

			bool success = false;

			std::string message;

			size_t filesCopied = 0;

			size_t streamCopied = 0;

			size_t scriptCopied = 0;

			std::filesystem::path outputPath;

		};



		struct DumpProgressState

		{

			bool active = false;

			float progress = 0.f;

			size_t filesWritten = 0;

			std::string phase;

			std::string detail;

		};



		std::filesystem::path GetFiveMDataRoot();

		DumpProgressState GetDumpProgress();

		std::vector<ServerSession> ListServerSessions();

		ServerSession GetActiveServerSession();

		ServerScanResult ScanActiveServer();

		ServerDumpResult DumpActiveServer(const std::filesystem::path& outputRoot, bool includeStreamables, bool includeScripts, bool includeAllFiles);

		struct RuntimeHarvestResult
		{
			bool success = false;
			std::string message;
			std::filesystem::path outputPath;
			size_t clientFiles = 0;
			size_t resourceCount = 0;
			std::vector<std::string> resourceNames;
		};

		RuntimeHarvestResult HarvestRuntimeClientScripts();
	}
}



