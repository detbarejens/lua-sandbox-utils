#include "BrandPaths.hpp"
#include "../Definations/Brand.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <fstream>
#include <vector>

namespace BrandPaths
{
	namespace
	{
		std::string g_cachedDataRoot;
		std::string g_cachedLockRoot;

		std::string EnsureTrailingSlash(std::string path)
		{
			if (!path.empty() && path.back() != '\\' && path.back() != '/')
				path.push_back('\\');
			return path;
		}

		bool CanWriteDirectory(const std::filesystem::path& directory)
		{
			std::error_code ec;
			std::filesystem::create_directories(directory, ec);
			if (ec)
				return false;

			const auto testFile = directory / ".hz_write_test";
			std::ofstream file(testFile, std::ios::binary | std::ios::trunc);
			if (!file)
				return false;

			file << '1';
			file.close();

			std::filesystem::remove(testFile, ec);
			return true;
		}

		std::string GetLocalAppDataRoot()
		{
			char appData[MAX_PATH]{};
			if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData)))
				return {};

			return EnsureTrailingSlash(std::string(appData) + "HZ\\Data");
		}

		std::string ResolveFirstWritableRoot(const std::vector<std::filesystem::path>& candidates, const std::string& forceFallback)
		{
			for (const auto& candidate : candidates)
			{
				if (candidate.empty())
					continue;

				if (CanWriteDirectory(candidate))
					return EnsureTrailingSlash(candidate.string());
			}

			if (!forceFallback.empty())
			{
				std::error_code ec;
				std::filesystem::create_directories(forceFallback, ec);
				return EnsureTrailingSlash(forceFallback);
			}

			return {};
		}

		void AppendExeRelativeCandidates(const std::string& exeDir, std::vector<std::filesystem::path>& candidates)
		{
			if (exeDir.empty())
				return;

			const auto exePath = std::filesystem::path(exeDir);
			std::error_code siblingEc;
			candidates.push_back(std::filesystem::weakly_canonical(exePath / ".." / "Data", siblingEc));
			candidates.push_back(exePath / "Data");
		}
	}

	void ResetCachedRoots()
	{
		g_cachedDataRoot.clear();
		g_cachedLockRoot.clear();
	}

	std::string GetExecutableDirectory()
	{
		char modulePath[MAX_PATH]{};
		if (!GetModuleFileNameA(nullptr, modulePath, MAX_PATH))
			return {};

		std::error_code ec;
		const auto directory = std::filesystem::path(modulePath).parent_path();
		const auto canonical = std::filesystem::weakly_canonical(directory, ec);
		if (ec)
			return EnsureTrailingSlash(directory.string());

		return EnsureTrailingSlash(canonical.string());
	}

	std::string GetLockDataRoot()
	{
		if (!g_cachedLockRoot.empty())
			return g_cachedLockRoot;

		std::vector<std::filesystem::path> candidates;
		const std::string appDataRoot = GetLocalAppDataRoot();
		const std::string exeDir = GetExecutableDirectory();

		// Retail exes are often sent alone — AppData is the most reliable lock location.
		if (!appDataRoot.empty())
			candidates.emplace_back(appDataRoot);

		AppendExeRelativeCandidates(exeDir, candidates);
		candidates.emplace_back(BRAND_DATA_ROOT);

		g_cachedLockRoot = ResolveFirstWritableRoot(candidates, appDataRoot);
		if (g_cachedLockRoot.empty() && !exeDir.empty())
			g_cachedLockRoot = EnsureTrailingSlash(exeDir + "Data");

		return g_cachedLockRoot;
	}

	std::string GetDataRoot()
	{
		if (!g_cachedDataRoot.empty())
			return g_cachedDataRoot;

		std::vector<std::filesystem::path> candidates;
		const std::string appDataRoot = GetLocalAppDataRoot();
		const std::string exeDir = GetExecutableDirectory();

		AppendExeRelativeCandidates(exeDir, candidates);

		if (!appDataRoot.empty())
			candidates.emplace_back(appDataRoot);

		candidates.emplace_back(BRAND_DATA_ROOT);

		g_cachedDataRoot = ResolveFirstWritableRoot(candidates, appDataRoot);
		if (g_cachedDataRoot.empty() && !exeDir.empty())
			g_cachedDataRoot = EnsureTrailingSlash(exeDir + "Data");

		return g_cachedDataRoot;
	}
}
