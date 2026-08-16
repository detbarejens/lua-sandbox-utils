#include "ServerDump.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "DevBridgeHttp.hpp"
#include "../../Utils/BrandPaths.hpp"
#include "../../Utils/Memory.hpp"
#include "../../json.hpp"

#include <Windows.h>
#include <ShlObj.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace Cheat
{
	namespace ServerDump
	{
		namespace
		{
			std::mutex g_dumpProgressMutex;
			std::atomic<bool> g_dumpProgressActive{ false };
			std::atomic<float> g_dumpProgressValue{ 0.f };
			std::atomic<size_t> g_dumpProgressFiles{ 0 };
			std::string g_dumpProgressPhase = "Idle";
			std::string g_dumpProgressDetail;

			void BeginDumpProgress()
			{
				std::lock_guard<std::mutex> lock(g_dumpProgressMutex);
				g_dumpProgressActive.store(true);
				g_dumpProgressValue.store(0.f);
				g_dumpProgressFiles.store(0);
				g_dumpProgressPhase = "Starting dump";
				g_dumpProgressDetail.clear();
			}

			void SetDumpProgress(float progress, const std::string& phase, const std::string& detail = {},
				size_t filesWritten = static_cast<size_t>(-1))
			{
				std::lock_guard<std::mutex> lock(g_dumpProgressMutex);
				g_dumpProgressActive.store(true);
				g_dumpProgressValue.store((std::max)(0.f, (std::min)(1.f, progress)));
				g_dumpProgressPhase = phase;
				if (!detail.empty())
					g_dumpProgressDetail = detail;
				if (filesWritten != static_cast<size_t>(-1))
					g_dumpProgressFiles.store(filesWritten);
			}

			void FinishDumpProgress(size_t filesWritten)
			{
				std::lock_guard<std::mutex> lock(g_dumpProgressMutex);
				g_dumpProgressValue.store(1.f);
				g_dumpProgressFiles.store(filesWritten);
				g_dumpProgressPhase = "Complete";
				g_dumpProgressActive.store(false);
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

			bool LineHasTrigger(const std::string& line)
			{
				return line.find("TriggerServerEvent") != std::string::npos ||
					line.find("TriggerEvent") != std::string::npos ||
					line.find("TriggerLatentServerEvent") != std::string::npos;
			}

			std::string ToLower(std::string value)
			{
				for (char& c : value)
					c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
				return value;
			}

			int64_t ToUnixTime(const std::filesystem::file_time_type& time)
			{
				const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
					time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
				return std::chrono::system_clock::to_time_t(sctp);
			}

			int64_t GetPathLastWriteUnix(const std::filesystem::path& path)
			{
				std::error_code ec;
				const auto time = std::filesystem::last_write_time(path, ec);
				if (ec)
					return 0;
				return ToUnixTime(time);
			}

			std::string ReadTextFileIfExists(const std::filesystem::path& path)
			{
				std::ifstream file(path, std::ios::binary);
				if (!file)
					return {};

				std::ostringstream ss;
				ss << file.rdbuf();
				return ss.str();
			}

			std::string DetectSessionLabel(const std::filesystem::path& cachePath)
			{
				const auto serverJson = ReadTextFileIfExists(cachePath / "server.json");
				if (!serverJson.empty())
				{
					const auto hostPos = serverJson.find("\"hostname\"");
					if (hostPos != std::string::npos)
					{
						const auto quoteStart = serverJson.find('"', hostPos + 10);
						const auto quoteEnd = quoteStart != std::string::npos ? serverJson.find('"', quoteStart + 1) : std::string::npos;
						if (quoteStart != std::string::npos && quoteEnd != std::string::npos && quoteEnd > quoteStart + 1)
							return serverJson.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
					}
				}

				const auto endpointFile = ReadTextFileIfExists(cachePath / "endpoint.txt");
				if (!endpointFile.empty())
					return TrimLine(endpointFile);

				return cachePath.filename().string();
			}

			std::string SanitizePathLabel(std::string label)
			{
				for (char& c : label)
				{
					switch (c)
					{
					case '\\': case '/': case ':': case '*': case '?':
					case '"': case '<': case '>': case '|':
						c = '_';
						break;
					default:
						break;
					}
				}

				if (label.empty())
					label = "server";
				return label;
			}

			bool IsCacheBlobName(const std::string& name)
			{
				return name.rfind("cache_", 0) == 0;
			}

			bool IsCacheBlobFile(const std::filesystem::path& path)
			{
				return IsCacheBlobName(path.filename().string());
			}

			bool IsLevelDbArtifact(const std::filesystem::path& path)
			{
				const std::string name = path.filename().string();
				const std::string ext = ToLower(path.extension().string());
				return ext == ".ldb" || ext == ".log" || name == "CURRENT" || name == "LOCK" || name.rfind("MANIFEST-", 0) == 0;
			}

			std::string ExtractJsonStringValue(const std::string& json, const std::string& key)
			{
				const auto keyPos = json.find('"' + key + '"');
				if (keyPos == std::string::npos)
					return {};

				const auto colon = json.find(':', keyPos + key.size() + 2);
				if (colon == std::string::npos)
					return {};

				const auto quoteStart = json.find('"', colon + 1);
				if (quoteStart == std::string::npos)
					return {};

				const auto quoteEnd = json.find('"', quoteStart + 1);
				if (quoteEnd == std::string::npos || quoteEnd <= quoteStart + 1)
					return {};

				return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
			}

			void ParseResourcesFromServerJson(const std::string& json, std::vector<DevResourceEntry>& resources)
			{
				const auto resourcesPos = json.find("\"resources\"");
				if (resourcesPos == std::string::npos)
					return;

				const auto arrayStart = json.find('[', resourcesPos);
				const auto arrayEnd = json.find(']', arrayStart);
				if (arrayStart == std::string::npos || arrayEnd == std::string::npos || arrayEnd <= arrayStart)
					return;

				const std::string slice = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
				std::set<std::string> seen;
				for (size_t pos = 0; pos < slice.size();)
				{
					const auto quoteStart = slice.find('"', pos);
					if (quoteStart == std::string::npos)
						break;

					const auto quoteEnd = slice.find('"', quoteStart + 1);
					if (quoteEnd == std::string::npos)
						break;

					const std::string name = slice.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
					if (!name.empty() && seen.insert(name).second)
					{
						DevResourceEntry entry;
						entry.name = name;
						entry.state = "Listed";
						entry.threadCount = 0;
						entry.eventCount = 0;
						resources.push_back(std::move(entry));
					}

					pos = quoteEnd + 1;
				}
			}

			bool IsReadableProtect(DWORD protect)
			{
				if (protect & PAGE_GUARD)
					return false;
				if (protect & PAGE_NOACCESS)
					return false;

				switch (protect & 0xFF)
				{
				case PAGE_READONLY:
				case PAGE_READWRITE:
				case PAGE_WRITECOPY:
				case PAGE_EXECUTE_READ:
				case PAGE_EXECUTE_READWRITE:
				case PAGE_EXECUTE_WRITECOPY:
					return true;
				default:
					return false;
				}
			}

			bool LineHasAnyTriggerPattern(const std::string& line)
			{
				return LineHasTrigger(line);
			}

			std::string ExtractTriggerLineFromMemory(const std::vector<uint8_t>& buffer, size_t matchIndex)
			{
				size_t start = matchIndex;
				while (start > 0 && buffer[start - 1] != '\n' && (matchIndex - start) < 512)
					--start;

				size_t end = matchIndex;
				while (end < buffer.size() && buffer[end] != '\n' && (end - matchIndex) < 512)
					++end;

				std::string line(reinterpret_cast<const char*>(buffer.data() + start), end - start);
				line = TrimLine(line);
				if (line.size() > 512)
					line.resize(512);
				return line;
			}

			std::string GuessResourceFromTriggerLine(const std::string& line, const std::vector<DevResourceEntry>& resources)
			{
				for (const auto& resource : resources)
				{
					if (resource.name.empty())
						continue;
					if (line.find(resource.name) != std::string::npos)
						return resource.name;
				}
				return "loaded";
			}

			void AddTriggerHit(const std::string& resource, const std::string& code,
				std::vector<DevTriggerEntry>& triggers, std::set<std::string>& seenTriggers)
			{
				if (code.empty())
					return;

				const std::string key = resource + "|" + code;
				if (!seenTriggers.insert(key).second)
					return;

				DevTriggerEntry trigger;
				trigger.resource = resource.empty() ? "unknown" : resource;
				trigger.code = code;
				triggers.push_back(std::move(trigger));
			}

			bool IsMostlyPrintableText(const std::string& text)
			{
				if (text.size() < 16)
					return false;

				size_t printable = 0;
				for (unsigned char c : text)
				{
					if (c == '\r' || c == '\n' || c == '\t' || c >= 32)
						++printable;
				}

				return printable * 100 / text.size() >= 75;
			}

			std::string ExtractResourceNameFromManifest(const std::string& manifest)
			{
				static const std::regex patterns[] = {
					std::regex(R"(name\s*['"]([^'"]+)['"])", std::regex::icase),
					std::regex(R"(resource\s*['"]([^'"]+)['"])", std::regex::icase),
				};

				for (const auto& pattern : patterns)
				{
					std::smatch match;
					if (std::regex_search(manifest, match, pattern) && match.size() > 1)
						return match[1].str();
				}

				return {};
			}

			std::string ExtractManifestFromMemory(const std::vector<uint8_t>& buffer, size_t markerIndex)
			{
				size_t start = markerIndex;
				while (start > 0 && buffer[start - 1] != '\0' && (markerIndex - start) < 4096)
					--start;

				size_t end = markerIndex;
				while (end < buffer.size() && buffer[end] != '\0' && (end - markerIndex) < 32768)
					++end;

				return std::string(reinterpret_cast<const char*>(buffer.data() + start), end - start);
			}

			struct MemoryDumpResult
			{
				size_t filesWritten = 0;
				size_t resources = 0;
			};

			bool IsPathChar(unsigned char c)
			{
				return std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '/' || c == '\\' ||
					c == '@' || c == ':';
			}

			bool HasClientSideExtension(const std::string& lower)
			{
				auto endsWith = [&](const char* ext) {
					const size_t n = std::strlen(ext);
					return lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0;
				};
				return endsWith(".lua") || endsWith(".json") || endsWith(".cfg") ||
					endsWith(".js") || endsWith(".html") || endsWith(".css") ||
					endsWith(".xml") || endsWith(".txt") || endsWith(".vue") ||
					endsWith(".ts") || endsWith(".md") || endsWith(".ysc");
			}

			bool IsServerOnlyPath(const std::string& lower)
			{
				if (lower.find("server/") != std::string::npos || lower.find("sv/") != std::string::npos)
					return true;
				if (lower == "server.lua" || lower == "sv.lua")
					return true;
				if (lower.size() >= 11 && lower.compare(lower.size() - 11, 11, "/server.lua") == 0)
					return true;
				if (lower.size() >= 7 && lower.compare(lower.size() - 7, 7, "/sv.lua") == 0)
					return true;
				if (lower.rfind("sv_", 0) == 0 || lower.rfind("server_", 0) == 0)
					return true;
				return false;
			}

			bool IsClientSidePath(const std::string& path)
			{
				const std::string lower = ToLower(path);
				if (lower == "fxmanifest.lua" || lower == "__resource.lua")
					return true;
				if (IsServerOnlyPath(lower))
					return false;
				if (lower.find("client/") != std::string::npos ||
					lower.find("shared/") != std::string::npos ||
					lower.find("html/") != std::string::npos ||
					lower.find("nui/") != std::string::npos ||
					lower.find("ui/") != std::string::npos ||
					lower.find("web/") != std::string::npos ||
					lower.find("config") != std::string::npos)
					return true;
				if (lower == "client.lua" || lower == "cl.lua" || lower == "shared.lua" ||
					lower.rfind("cl_", 0) == 0 || lower.rfind("client_", 0) == 0)
					return true;
				return HasClientSideExtension(lower);
			}

			bool MatchesSimpleGlob(const std::string& path, const std::string& glob)
			{
				std::string lowerPath = ToLower(path);
				std::string lowerGlob = ToLower(glob);
				size_t pos = 0;
				while ((pos = lowerGlob.find("**/*", pos)) != std::string::npos)
					lowerGlob.replace(pos, 4, "*");
				pos = 0;
				while ((pos = lowerGlob.find("**", pos)) != std::string::npos)
					lowerGlob.replace(pos, 2, "*");

				const char* p = lowerPath.c_str();
				const char* g = lowerGlob.c_str();
				const char* star = nullptr;
				const char* saved = p;
				while (*p)
				{
					if (*g == '*')
					{
						star = g++;
						saved = p;
						continue;
					}
					if (*g == *p || *g == '?')
					{
						++g;
						++p;
						continue;
					}
					if (star)
					{
						g = star + 1;
						p = ++saved;
						continue;
					}
					return false;
				}
				while (*g == '*')
					++g;
				return *g == 0;
			}

			bool IsInterestingScriptPath(const std::string& path, bool includeScripts, bool includeAllFiles)
			{
				if (includeAllFiles)
					return true;
				if (IsClientSidePath(path))
					return true;
				return includeScripts && HasClientSideExtension(ToLower(path)) && !IsServerOnlyPath(ToLower(path));
			}

			std::vector<std::string> ParseManifestScriptPaths(const std::string& manifest)
			{
				std::vector<std::string> paths;
				std::set<std::string> seen;

				auto normalizePath = [](std::string path) -> std::string
				{
					while (!path.empty() && std::isspace(static_cast<unsigned char>(path.front())))
						path.erase(path.begin());
					while (!path.empty() && std::isspace(static_cast<unsigned char>(path.back())))
						path.pop_back();
					if (!path.empty() && (path.front() == '"' || path.front() == '\''))
						path.erase(path.begin());
					if (!path.empty() && (path.back() == '"' || path.back() == '\''))
						path.pop_back();
					const auto atPos = path.find('@');
					if (atPos != std::string::npos)
					{
						const auto slash = path.find('/', atPos);
						if (slash != std::string::npos)
							path = path.substr(slash + 1);
					}
					return path;
				};

				auto addPath = [&](const std::string& path)
				{
					const std::string normalized = normalizePath(path);
					if (normalized.empty())
						return;
					const std::string key = (normalized.find('*') != std::string::npos ? "glob:" : "") + normalized;
					if (seen.insert(key).second)
						paths.push_back(normalized);
				};

				static const std::regex blockPattern(
					R"((client_scripts|client_script|shared_scripts|shared_script|server_scripts|server_script|files|ui_page|data_file)\s*[\{\[]([^\}\]]+)[\}\]])",
					std::regex::icase);
				static const std::regex quotedPath(R"(['"]([^'"]+)['"])");

				for (auto it = std::sregex_iterator(manifest.begin(), manifest.end(), blockPattern);
					it != std::sregex_iterator(); ++it)
				{
					const std::string block = (*it)[2].str();
					for (auto pathIt = std::sregex_iterator(block.begin(), block.end(), quotedPath);
						pathIt != std::sregex_iterator(); ++pathIt)
					{
						addPath((*pathIt)[1].str());
					}
				}

				static const std::regex singlePattern(
					R"((client_scripts|client_script|shared_scripts|shared_script|server_scripts|server_script|files|ui_page|data_file)\s+['"]([^'"]+)['"])",
					std::regex::icase);
				for (auto it = std::sregex_iterator(manifest.begin(), manifest.end(), singlePattern);
					it != std::sregex_iterator(); ++it)
				{
					addPath((*it)[2].str());
				}

				static const std::regex quoted(R"(['"]([^'"]+\.(lua|json|cfg|js|html|css|xml|txt|md))['"])", std::regex::icase);
				for (auto it = std::sregex_iterator(manifest.begin(), manifest.end(), quoted);
					it != std::sregex_iterator(); ++it)
				{
					addPath((*it)[1].str());
				}

				static const char* defaults[] = {
					"config.lua", "config.json", "shared/config.lua", "shared/config.json",
					"client/config.lua", "configs/config.lua", "configs/config.json",
					"settings.lua", "shared/settings.lua",
				};
				for (const char* path : defaults)
					addPath(path);

				return paths;
			}

			bool IsManifestContent(const std::string& content)
			{
				return content.find("fx_version") != std::string::npos ||
					content.find("resource_manifest_version") != std::string::npos ||
					(content.find("files") != std::string::npos &&
						content.find("dependency") != std::string::npos &&
						content.find("function") == std::string::npos);
			}

			bool LooksLikeLuaSource(const std::string& content);

			bool ShouldIncludeManifestPath(const std::string& path, bool includeStreamables, bool includeScripts, bool includeAllFiles)
			{
				if (includeAllFiles)
					return true;

				const std::string lower = ToLower(path);
				if (IsServerOnlyPath(lower) && !includeAllFiles)
					return false;
				if (IsClientSidePath(path))
					return true;

				if (includeScripts && HasClientSideExtension(lower))
					return true;

				if (includeStreamables)
				{
					if (lower.find("stream/") != std::string::npos)
						return true;
					if (lower.size() > 4 && (
						lower.rfind(".yft") == lower.size() - 4 ||
						lower.rfind(".ytd") == lower.size() - 4 ||
						lower.rfind(".ydr") == lower.size() - 4 ||
						lower.rfind(".meta") == lower.size() - 5 ||
						lower.rfind(".ymap") == lower.size() - 5 ||
						lower.rfind(".rpf") == lower.size() - 4))
						return true;
				}

				return false;
			}

			bool ContentMatchesPath(const std::string& relativePath, const std::string& content)
			{
				if (content.empty() || !IsMostlyPrintableText(content))
					return false;

				const std::string lower = ToLower(relativePath);
				const bool manifestPath = lower == "fxmanifest.lua" || lower == "__resource.lua";
				const bool manifestContent = IsManifestContent(content);

				if (manifestPath)
					return manifestContent;

				if (manifestContent)
					return false;

				if (lower.rfind(".json") == lower.size() - 5)
					return content.find('{') != std::string::npos || content.find('[') != std::string::npos;

				if (lower.rfind(".lua") == lower.size() - 4)
					return LooksLikeLuaSource(content);

				if (lower.rfind(".js") == lower.size() - 3)
					return content.find("function") != std::string::npos || content.find("const ") != std::string::npos;

				if (lower.rfind(".html") == lower.size() - 5)
					return content.find('<') != std::string::npos;

				if (lower.rfind(".css") == lower.size() - 4)
					return content.find('{') != std::string::npos;

				return IsMostlyPrintableText(content);
			}

			std::string ResolveResourceName(const std::string& manifest, const std::vector<DevResourceEntry>& resources)
			{
				std::string resourceName = ExtractResourceNameFromManifest(manifest);
				if (!resourceName.empty())
					return resourceName;

				for (const auto& resource : resources)
				{
					if (resource.name.empty())
						continue;
					if (manifest.find(resource.name) != std::string::npos)
						return resource.name;
				}

				return {};
			}

			bool LooksLikeLuaSource(const std::string& content)
			{
				if (content.size() < 24 || !IsMostlyPrintableText(content))
					return false;

				return content.find("function") != std::string::npos ||
					content.find("local ") != std::string::npos ||
					content.find("return ") != std::string::npos ||
					content.find("Config") != std::string::npos ||
					content.find("RegisterNetEvent") != std::string::npos ||
					content.find("CreateThread") != std::string::npos ||
					content.find("AddEventHandler") != std::string::npos ||
					content.find("exports[") != std::string::npos ||
					content.find("TriggerServerEvent") != std::string::npos ||
					content.find("TriggerEvent") != std::string::npos ||
					content.find("ESX") != std::string::npos ||
					content.find("QBCore") != std::string::npos ||
					content.find("lib.") != std::string::npos ||
					content.find("fx_version") != std::string::npos;
			}

			bool WriteDumpFile(const std::filesystem::path& outputPath, const std::string& resourceName,
				const std::string& relativePath, const std::string& content, std::set<std::string>& seenFiles,
				MemoryDumpResult& result, std::set<std::string>& resourceNames)
			{
				if (content.empty() || !ContentMatchesPath(relativePath, content))
					return false;

				const std::string fileKey = resourceName + "|" + relativePath;
				if (!seenFiles.insert(fileKey).second)
					return false;

				std::filesystem::path target = outputPath / SanitizePathLabel(resourceName);
				for (const auto part : std::filesystem::path(relativePath))
					target /= part;

				std::error_code ec;
				std::filesystem::create_directories(target.parent_path(), ec);
				std::ofstream out(target, std::ios::binary | std::ios::trunc);
				if (!out)
					return false;

				out << content;
				++result.filesWritten;
				resourceNames.insert(resourceName);
				return true;
			}

			struct PathHit
			{
				std::string resource;
				std::string relativePath;
				uintptr_t address = 0;
				size_t length = 0;
			};

			bool ValidUserPtr(uintptr_t address)
			{
				return address >= 0x10000 && address <= 0x7FFFFFFFFFFFULL;
			}

			template <typename T>
			T ReadRemoteValue(uintptr_t address)
			{
				T value{};
				SIZE_T read = 0;
				ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
					reinterpret_cast<LPCVOID>(address), &value, sizeof(T), &read);
				return value;
			}

			std::string ReadRemoteBytes(uintptr_t address, size_t size)
			{
				if (!ValidUserPtr(address) || size < 8 || size > 2ull * 1024ull * 1024ull)
					return {};

				std::string out(size, '\0');
				SIZE_T read = 0;
				if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
					reinterpret_cast<LPCVOID>(address), out.data(), size, &read) || read < 8)
					return {};
				out.resize(read);
				return out;
			}

			std::string ReadRemoteZString(uintptr_t address, size_t maxSize = 262144)
			{
				if (!ValidUserPtr(address))
					return {};

				std::string out;
				out.reserve(4096);
				uint8_t chunk[4096];
				uintptr_t cursor = address;
				while (out.size() < maxSize)
				{
					SIZE_T read = 0;
					if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
						reinterpret_cast<LPCVOID>(cursor), chunk, sizeof(chunk), &read) || read == 0)
						break;

					size_t take = 0;
					while (take < read && chunk[take] != 0)
						++take;
					out.append(reinterpret_cast<const char*>(chunk), take);
					if (take < read)
						break;
					cursor += read;
				}

				return out.size() >= 16 ? out : std::string{};
			}

			bool LooksLikeMsvcStringObject(uintptr_t objectAddr, uintptr_t expectedBuf, size_t expectedLen)
			{
				const size_t size = ReadRemoteValue<size_t>(objectAddr + 16);
				const size_t capacity = ReadRemoteValue<size_t>(objectAddr + 24);
				if (size != expectedLen || capacity < size || capacity > 1ull * 1024ull * 1024ull)
					return false;

				if (capacity < 16)
					return objectAddr == expectedBuf;

				const uintptr_t ptr = ReadRemoteValue<uintptr_t>(objectAddr);
				return ptr == expectedBuf;
			}

			std::string TryReadMsvcVector(uintptr_t vectorAddr)
			{
				const uintptr_t first = ReadRemoteValue<uintptr_t>(vectorAddr);
				const uintptr_t last = ReadRemoteValue<uintptr_t>(vectorAddr + 8);
				const uintptr_t end = ReadRemoteValue<uintptr_t>(vectorAddr + 16);
				if (!ValidUserPtr(first) || last < first || end < last)
					return {};

				const size_t size = static_cast<size_t>(last - first);
				const size_t capacity = static_cast<size_t>(end - first);
				if (size < 16 || size > 2ull * 1024ull * 1024ull || capacity < size || capacity > 8ull * 1024ull * 1024ull)
					return {};

				return ReadRemoteBytes(first, size);
			}

			std::string TryReadMsvcString(uintptr_t objectAddr)
			{
				const size_t size = ReadRemoteValue<size_t>(objectAddr + 16);
				const size_t capacity = ReadRemoteValue<size_t>(objectAddr + 24);
				if (size < 16 || size > 2ull * 1024ull * 1024ull || capacity < size || capacity > 8ull * 1024ull * 1024ull)
					return {};

				const uintptr_t buf = capacity < 16 ? objectAddr : ReadRemoteValue<uintptr_t>(objectAddr);
				if (!ValidUserPtr(buf))
					return {};
				return ReadRemoteBytes(buf, size);
			}

			std::string TryReadPtrSize(uintptr_t addr)
			{
				const uintptr_t ptr = ReadRemoteValue<uintptr_t>(addr);
				const size_t size = ReadRemoteValue<size_t>(addr + 8);
				if (!ValidUserPtr(ptr) || size < 16 || size > 2ull * 1024ull * 1024ull)
					return {};
				return ReadRemoteBytes(ptr, size);
			}

			std::string TryContentAt(uintptr_t addr, const std::string& relativePath)
			{
				std::string content = TryReadMsvcVector(addr);
				if (ContentMatchesPath(relativePath, content))
					return content;

				content = TryReadMsvcString(addr);
				if (ContentMatchesPath(relativePath, content))
					return content;

				content = TryReadPtrSize(addr);
				if (ContentMatchesPath(relativePath, content))
					return content;

				const uintptr_t ptr = ReadRemoteValue<uintptr_t>(addr);
				if (ValidUserPtr(ptr))
				{
					content = TryReadMsvcVector(ptr);
					if (ContentMatchesPath(relativePath, content))
						return content;
					content = TryReadMsvcString(ptr);
					if (ContentMatchesPath(relativePath, content))
						return content;
					content = TryReadPtrSize(ptr);
					if (ContentMatchesPath(relativePath, content))
						return content;
					content = ReadRemoteZString(ptr);
					if (ContentMatchesPath(relativePath, content))
						return content;
				}

				return {};
			}

			std::string ExtractContentFromStringObject(uintptr_t stringObj, const std::string& relativePath)
			{
				static const int extras[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96 };
				for (int extra : extras)
				{
					const std::string content = TryContentAt(stringObj + 32 + extra, relativePath);
					if (!content.empty())
						return content;
				}
				return {};
			}

			std::string ExtractContentForPathHit(const PathHit& hit)
			{
				if (!hit.address || hit.length == 0)
					return {};

				if (LooksLikeMsvcStringObject(hit.address, hit.address, hit.length))
				{
					const std::string content = ExtractContentFromStringObject(hit.address, hit.relativePath);
					if (!content.empty())
						return content;
				}

				for (int delta = -32; delta <= 32; delta += 8)
				{
					if (delta == 0)
						continue;
					const uintptr_t objectAddr = hit.address + static_cast<uintptr_t>(delta);
					if (!ValidUserPtr(objectAddr))
						continue;
					if (!LooksLikeMsvcStringObject(objectAddr, hit.address, hit.length))
						continue;
					const std::string content = ExtractContentFromStringObject(objectAddr, hit.relativePath);
					if (!content.empty())
						return content;
				}

				return {};
			}

			std::string NormalizeResourceRelativePath(std::string path, const std::string& resourceName)
			{
				std::replace(path.begin(), path.end(), '\\', '/');
				while (path.rfind("./", 0) == 0)
					path.erase(0, 2);

				const std::string resourcesPrefix = "resources:/";
				if (path.size() >= resourcesPrefix.size() &&
					ToLower(path.substr(0, resourcesPrefix.size())) == resourcesPrefix)
					path = path.substr(resourcesPrefix.size());

				if (!path.empty() && path.front() == '@')
				{
					const auto slash = path.find('/');
					if (slash != std::string::npos)
						path = path.substr(slash + 1);
				}

				if (!resourceName.empty() && path.size() > resourceName.size() + 1)
				{
					const std::string prefix = resourceName + "/";
					if (_strnicmp(path.c_str(), prefix.c_str(), prefix.size()) == 0)
						path = path.substr(prefix.size());
				}

				return path;
			}

			std::string GuessResourceFromAtPath(const std::string& path, const std::vector<DevResourceEntry>& resources)
			{
				std::string normalized = path;
				std::replace(normalized.begin(), normalized.end(), '\\', '/');
				if (!normalized.empty() && normalized.front() == '@')
					normalized.erase(normalized.begin());

				const std::string resourcesPrefix = "resources:/";
				if (normalized.size() >= resourcesPrefix.size() &&
					ToLower(normalized.substr(0, resourcesPrefix.size())) == resourcesPrefix)
					normalized = normalized.substr(resourcesPrefix.size());

				const auto slash = normalized.find('/');
				if (slash != std::string::npos && slash > 0 && slash < 64)
				{
					const std::string prefix = normalized.substr(0, slash);
					for (const auto& resource : resources)
					{
						if (_stricmp(resource.name.c_str(), prefix.c_str()) == 0)
							return resource.name;
					}
					return prefix;
				}

				return {};
			}

			std::string ExtractPathAtExtension(const std::vector<uint8_t>& buffer, size_t extDotIndex, const char* extension)
			{
				const size_t extLen = std::strlen(extension);
				if (extDotIndex + extLen > buffer.size())
					return {};

				if (std::memcmp(buffer.data() + extDotIndex, extension, extLen) != 0)
					return {};

				size_t start = extDotIndex;
				while (start > 0 && IsPathChar(buffer[start - 1]) && (extDotIndex - start) < 220)
					--start;

				if (extDotIndex <= start)
					return {};

				return std::string(reinterpret_cast<const char*>(buffer.data() + start), extDotIndex - start + extLen);
			}

			std::string ExtractContentAfterPath(const std::vector<uint8_t>& buffer, size_t pathEndIndex, size_t maxSize = 262144)
			{
				size_t start = pathEndIndex;
				while (start < buffer.size() && (buffer[start] == 0 || buffer[start] == '\r' || buffer[start] == '\n' ||
					buffer[start] == ' ' || buffer[start] == '"' || buffer[start] == '\''))
					++start;

				size_t end = start;
				while (end < buffer.size() && (end - start) < maxSize)
				{
					const unsigned char c = buffer[end];
					if (c == 0)
						break;
					if (c < 32 && c != '\r' && c != '\n' && c != '\t')
					{
						if ((end - start) > 48)
							break;
						++end;
						continue;
					}
					++end;
				}

				if (end <= start || (end - start) < 16)
					return {};

				return std::string(reinterpret_cast<const char*>(buffer.data() + start), end - start);
			}

			std::string ExtractNearbyScriptContent(const std::vector<uint8_t>& buffer, size_t pathEndIndex, const std::string& relativePath)
			{
				std::string immediate = ExtractContentAfterPath(buffer, pathEndIndex);
				if (ContentMatchesPath(relativePath, immediate))
					return immediate;

				size_t cursor = pathEndIndex;
				for (int attempt = 0; attempt < 12 && cursor < buffer.size() && (cursor - pathEndIndex) < 8192; ++attempt)
				{
					while (cursor < buffer.size() && buffer[cursor] == 0)
						++cursor;
					if (cursor >= buffer.size())
						break;

					const std::string candidate = ExtractContentAfterPath(buffer, cursor, 196608);
					if (ContentMatchesPath(relativePath, candidate))
						return candidate;

					cursor += (std::max)(candidate.size(), static_cast<size_t>(1));
				}

				return immediate.size() >= 24 ? immediate : std::string{};
			}

			std::string GuessResourceForPath(const std::string& path, const std::vector<DevResourceEntry>& resources)
			{
				for (const auto& resource : resources)
				{
					if (resource.name.empty())
						continue;
					if (path.find(resource.name + "/") == 0 || path.find(resource.name + "\\") == 0)
						return resource.name;
					if (path.find("/" + resource.name + "/") != std::string::npos)
						return resource.name;
				}

				const auto slash = path.find('/');
				if (slash != std::string::npos && slash > 0 && slash < 64)
				{
					const std::string prefix = path.substr(0, slash);
					for (const auto& resource : resources)
					{
						if (resource.name == prefix)
							return prefix;
					}
					return prefix;
				}

				return "memory";
			}

			void ScanBufferForScriptPaths(const std::vector<uint8_t>& buffer, size_t bytesRead,
				const std::filesystem::path& outputPath, const std::vector<DevResourceEntry>& resources,
				std::set<std::string>& seenFiles, MemoryDumpResult& result, std::set<std::string>& resourceNames,
				bool includeScripts, bool includeAllFiles)
			{
				static const char* extensions[] = { ".lua", ".json", ".cfg", ".js", ".html", ".css", ".xml", ".txt" };

				for (const char* extension : extensions)
				{
					const size_t extLen = std::strlen(extension);
					if (bytesRead < extLen)
						continue;

					for (size_t index = 0; index + extLen <= bytesRead; ++index)
					{
						if (std::memcmp(buffer.data() + index, extension, extLen) != 0)
							continue;

						const std::string relativePath = ExtractPathAtExtension(buffer, index, extension);
						if (relativePath.empty() || !IsInterestingScriptPath(relativePath, includeScripts, includeAllFiles))
							continue;

						const std::string content = ExtractNearbyScriptContent(buffer, index + extLen, relativePath);
						if (!LooksLikeLuaSource(content) && !IsMostlyPrintableText(content))
							continue;

						const std::string resourceName = GuessResourceForPath(relativePath, resources);
						WriteDumpFile(outputPath, resourceName, relativePath, content, seenFiles, result, resourceNames);
					}
				}
			}

			void ScanBufferForWantedPaths(const std::vector<uint8_t>& buffer, size_t bytesRead,
				const std::filesystem::path& outputPath,
				const std::vector<std::tuple<std::string, std::string, bool>>& wantedPaths,
				std::set<std::string>& seenFiles, MemoryDumpResult& result, std::set<std::string>& resourceNames)
			{
				if (wantedPaths.empty() || bytesRead < 16)
					return;

				const char* data = reinterpret_cast<const char*>(buffer.data());
				const std::string haystack(data, bytesRead);

				for (const auto& wanted : wantedPaths)
				{
					const std::string& resourceName = std::get<0>(wanted);
					const std::string& relativePath = std::get<1>(wanted);
					const bool isManifest = std::get<2>(wanted);
					if (relativePath.size() < 3)
						continue;

					size_t searchPos = 0;
					while ((searchPos = haystack.find(relativePath, searchPos)) != std::string::npos)
					{
						const std::string content = ExtractNearbyScriptContent(buffer, searchPos + relativePath.size(), relativePath);
						if (!ContentMatchesPath(isManifest ? "fxmanifest.lua" : relativePath, content))
						{
							searchPos += relativePath.size();
							continue;
						}

						if (WriteDumpFile(outputPath, resourceName, relativePath, content, seenFiles, result, resourceNames))
							break;

						searchPos += relativePath.size();
					}
				}
			}

			std::string GuessResourceForContent(const std::string& content,
				const std::vector<DevResourceEntry>& resources, const std::set<std::string>& resourceNames)
			{
				std::string best;
				auto consider = [&](const std::string& name)
				{
					if (name.empty() || name.rfind("resource_", 0) == 0)
						return;
					if (content.find("@" + name + "/") == std::string::npos &&
						content.find(name + "/") == std::string::npos &&
						content.find("'" + name + "'") == std::string::npos)
						return;
					if (name.size() > best.size())
						best = name;
				};

				for (const auto& name : resourceNames)
					consider(name);
				for (const auto& resource : resources)
					consider(resource.name);
				return best.empty() ? "memory" : best;
			}

			void ScanBufferForLuaBlocks(const std::vector<uint8_t>& buffer, size_t bytesRead,
				const std::filesystem::path& outputPath, std::set<std::string>& seenFiles,
				MemoryDumpResult& result, std::set<std::string>& resourceNames,
				const std::vector<DevResourceEntry>& resources, size_t& anonymousIndex)
			{
				size_t index = 0;
				while (index < bytesRead)
				{
					while (index < bytesRead && buffer[index] == 0)
						++index;
					const size_t start = index;
					while (index < bytesRead && buffer[index] != 0)
						++index;
					if (index <= start + 64 || (index - start) > 262144)
						continue;

					if (anonymousIndex > 2500)
						return;

					const std::string content(reinterpret_cast<const char*>(buffer.data() + start), index - start);
					if (IsManifestContent(content) || !LooksLikeLuaSource(content))
						continue;

					std::string relativePath;
					if (start > 8)
					{
						size_t pathEnd = start;
						while (pathEnd > 0 && buffer[pathEnd - 1] == 0)
							--pathEnd;
						size_t pathStart = pathEnd;
						while (pathStart > 0 && IsPathChar(buffer[pathStart - 1]) && (pathEnd - pathStart) < 220)
							--pathStart;
						if (pathEnd > pathStart + 4)
						{
							std::string maybePath(reinterpret_cast<const char*>(buffer.data() + pathStart), pathEnd - pathStart);
							std::replace(maybePath.begin(), maybePath.end(), '\\', '/');
							if (HasClientSideExtension(ToLower(maybePath)))
							{
								const std::string resource = GuessResourceFromAtPath(maybePath, resources).empty()
									? GuessResourceForPath(maybePath, resources)
									: GuessResourceFromAtPath(maybePath, resources);
								relativePath = NormalizeResourceRelativePath(maybePath, resource);
								if (!relativePath.empty() && IsInterestingScriptPath(relativePath, true, false))
								{
									WriteDumpFile(outputPath, resource.empty() ? "memory" : resource, relativePath,
										content, seenFiles, result, resourceNames);
									continue;
								}
							}
						}
					}

					const std::string resource = GuessResourceForContent(content, resources, resourceNames);
					relativePath = "client/recovered_" + std::to_string(++anonymousIndex) + ".lua";
					WriteDumpFile(outputPath, resource, relativePath, content, seenFiles, result, resourceNames);
				}
			}

			MemoryDumpResult DumpScriptsFromProcessMemory(const std::filesystem::path& outputPath,
				const std::vector<DevResourceEntry>& resources,
				bool includeStreamables, bool includeScripts, bool includeAllFiles,
				float progressStart, float progressEnd)
			{
				(void)includeStreamables;
				MemoryDumpResult result;
				if (!FrameWork::Memory::AttachedProcessHandle || !FrameWork::Memory::AttachedProcessPid)
					return result;

				static const char* markers[] = {
					"fx_version",
					"resource_manifest_version",
				};

				SYSTEM_INFO systemInfo{};
				GetSystemInfo(&systemInfo);

				const uint64_t minAddress = reinterpret_cast<uint64_t>(systemInfo.lpMinimumApplicationAddress);
				const uint64_t maxAddress = reinterpret_cast<uint64_t>(systemInfo.lpMaximumApplicationAddress);
				const size_t chunkSize = 1024 * 1024;
				const float progressSpan = progressEnd - progressStart;

				std::vector<uint8_t> buffer(chunkSize);
				std::set<std::string> seenManifests;
				std::set<std::string> seenFiles;
				std::set<std::string> resourceNames;
				std::vector<std::tuple<std::string, std::string, bool>> wantedPaths;
				std::vector<PathHit> pathHits;
				std::unordered_map<uintptr_t, size_t> pathByAddress;
				size_t anonymousLuaIndex = 0;

				SetDumpProgress(progressStart, "Finding client scripts in FiveM memory");

				std::vector<std::pair<std::string, std::string>> globWanted;

				auto considerPath = [&](std::string resourceName, const std::string& rawPath, uintptr_t address)
				{
					if (rawPath.size() < 5 || pathHits.size() > 80000)
						return;

					if (resourceName.empty())
					{
						resourceName = GuessResourceFromAtPath(rawPath, resources);
						if (resourceName.empty())
							resourceName = GuessResourceForPath(rawPath, resources);
					}

					const std::string relativePath = NormalizeResourceRelativePath(rawPath, resourceName);
					if (relativePath.empty() || !IsInterestingScriptPath(relativePath, includeScripts, includeAllFiles))
						return;

					PathHit hit;
					hit.resource = resourceName.empty() ? "memory" : resourceName;
					hit.relativePath = relativePath;
					hit.address = address;
					hit.length = rawPath.size();
					pathByAddress[address] = pathHits.size();
					pathHits.push_back(hit);

					const std::string content = ExtractContentForPathHit(hit);
					if (!content.empty())
						WriteDumpFile(outputPath, hit.resource, hit.relativePath, content, seenFiles, result, resourceNames);
				};

				auto walkMemory = [&](const auto& onChunk)
				{
					for (uint64_t address = minAddress; address < maxAddress;)
					{
						MEMORY_BASIC_INFORMATION memoryInfo{};
						if (!VirtualQueryEx(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<LPCVOID>(address), &memoryInfo, sizeof(memoryInfo)))
						{
							address += systemInfo.dwPageSize;
							continue;
						}

						const uint64_t regionBase = reinterpret_cast<uint64_t>(memoryInfo.BaseAddress);
						const size_t regionSize = static_cast<size_t>(memoryInfo.RegionSize);
						const bool readable = memoryInfo.State == MEM_COMMIT && IsReadableProtect(memoryInfo.Protect);

						if (readable && regionSize >= 4096 && regionSize <= (256ull * 1024ull * 1024ull))
						{
							for (size_t offset = 0; offset < regionSize; offset += chunkSize)
							{
								const size_t readSize = (std::min)(chunkSize, regionSize - offset);
								SIZE_T bytesRead = 0;
								if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
									reinterpret_cast<LPCVOID>(regionBase + offset),
									buffer.data(), readSize, &bytesRead) || bytesRead == 0)
								{
									continue;
								}

								buffer.resize(bytesRead);
								onChunk(regionBase + offset, bytesRead);
								buffer.resize(chunkSize);
							}
						}

						address = regionBase + regionSize;
					}
				};

				walkMemory([&](uintptr_t chunkBase, size_t bytesRead)
				{
					for (const char* marker : markers)
					{
						const size_t markerLen = std::strlen(marker);
						if (markerLen == 0 || bytesRead < markerLen)
							continue;

						for (size_t index = 0; index + markerLen <= bytesRead; ++index)
						{
							if (std::memcmp(buffer.data() + index, marker, markerLen) != 0)
								continue;

							const std::string manifest = ExtractManifestFromMemory(buffer, index);
							if (!IsManifestContent(manifest) || !IsMostlyPrintableText(manifest))
								continue;

							const std::string manifestKey = std::to_string(manifest.size()) + "|" + manifest.substr(0, 120);
							if (!seenManifests.insert(manifestKey).second)
								continue;

							std::string resourceName = ResolveResourceName(manifest, resources);
							if (resourceName.empty())
								resourceName = "resource_" + std::to_string(seenManifests.size());

							WriteDumpFile(outputPath, resourceName, "fxmanifest.lua", manifest, seenFiles, result, resourceNames);
							for (const auto& scriptPath : ParseManifestScriptPaths(manifest))
							{
								if (scriptPath.find('*') != std::string::npos)
								{
									globWanted.emplace_back(resourceName, scriptPath);
									continue;
								}
								if (ShouldIncludeManifestPath(scriptPath, includeStreamables, includeScripts, includeAllFiles))
									wantedPaths.emplace_back(resourceName, scriptPath, false);
							}
						}
					}

					static const char* extensions[] = { ".lua", ".json", ".cfg", ".js", ".html", ".css", ".xml", ".vue" };
					for (const char* extension : extensions)
					{
						const size_t extLen = std::strlen(extension);
						if (bytesRead < extLen)
							continue;

						for (size_t index = 0; index + extLen <= bytesRead; ++index)
						{
							if (std::memcmp(buffer.data() + index, extension, extLen) != 0)
								continue;

							std::string rawPath = ExtractPathAtExtension(buffer, index, extension);
							if (rawPath.empty())
								continue;

							const size_t pathStart = index - (rawPath.size() - extLen);
							considerPath({}, rawPath, chunkBase + pathStart);
						}
					}
				});

				std::set<std::string> wantedKeys;
				for (const auto& wanted : wantedPaths)
					wantedKeys.insert(std::get<0>(wanted) + "|" + std::get<1>(wanted));

				for (const auto& hit : pathHits)
				{
					bool matchedGlob = false;
					for (const auto& glob : globWanted)
					{
						if ((glob.first == hit.resource || glob.first.empty()) &&
							MatchesSimpleGlob(hit.relativePath, glob.second))
						{
							matchedGlob = true;
							break;
						}
					}

					if (matchedGlob || IsClientSidePath(hit.relativePath))
					{
						const std::string key = hit.resource + "|" + hit.relativePath;
						if (wantedKeys.insert(key).second)
							wantedPaths.emplace_back(hit.resource, hit.relativePath, false);
					}
				}

				SetDumpProgress(progressStart + progressSpan * 0.45f, "Extracting client-side file contents",
					std::to_string(wantedPaths.size()) + " client paths", result.filesWritten);

				walkMemory([&](uintptr_t chunkBase, size_t bytesRead)
				{
					const size_t aligned = bytesRead & ~size_t(7);
					for (size_t offset = 0; offset + 8 <= aligned; offset += 8)
					{
						const uintptr_t value = *reinterpret_cast<const uintptr_t*>(buffer.data() + offset);
						const auto found = pathByAddress.find(value);
						if (found == pathByAddress.end())
							continue;

						const PathHit& hit = pathHits[found->second];
						const uintptr_t objectAddr = chunkBase + offset;
						if (!LooksLikeMsvcStringObject(objectAddr, hit.address, hit.length))
							continue;

						const std::string content = ExtractContentFromStringObject(objectAddr, hit.relativePath);
						if (!content.empty())
							WriteDumpFile(outputPath, hit.resource, hit.relativePath, content, seenFiles, result, resourceNames);
					}

					ScanBufferForWantedPaths(buffer, bytesRead, outputPath, wantedPaths, seenFiles, result, resourceNames);
					ScanBufferForScriptPaths(buffer, bytesRead, outputPath, resources, seenFiles, result, resourceNames,
						includeScripts, includeAllFiles);
					ScanBufferForLuaBlocks(buffer, bytesRead, outputPath, seenFiles, result, resourceNames,
						resources, anonymousLuaIndex);
				});

				SetDumpProgress(progressEnd, "Memory scan complete",
					std::to_string(result.filesWritten) + " client files found", result.filesWritten);
				result.resources = resourceNames.size();
				return result;
			}

			void ScanProcessMemoryForTriggers(const std::vector<DevResourceEntry>& resources,
				std::vector<DevTriggerEntry>& triggers, std::set<std::string>& seenTriggers)
			{
				if (!FrameWork::Memory::AttachedProcessHandle || !FrameWork::Memory::AttachedProcessPid)
					return;

				static const char* patterns[] = {
					"TriggerServerEvent",
					"TriggerEvent",
					"TriggerLatentServerEvent"
				};

				SYSTEM_INFO systemInfo{};
				GetSystemInfo(&systemInfo);

				const uint64_t minAddress = reinterpret_cast<uint64_t>(systemInfo.lpMinimumApplicationAddress);
				const uint64_t maxAddress = reinterpret_cast<uint64_t>(systemInfo.lpMaximumApplicationAddress);
				const size_t chunkSize = 1024 * 1024;

				std::vector<uint8_t> buffer(chunkSize);

				for (uint64_t address = minAddress; address < maxAddress;)
				{
					MEMORY_BASIC_INFORMATION memoryInfo{};
					if (!VirtualQueryEx(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<LPCVOID>(address), &memoryInfo, sizeof(memoryInfo)))
					{
						address += systemInfo.dwPageSize;
						continue;
					}

					const uint64_t regionBase = reinterpret_cast<uint64_t>(memoryInfo.BaseAddress);
					const size_t regionSize = static_cast<size_t>(memoryInfo.RegionSize);
					const bool readable = memoryInfo.State == MEM_COMMIT && IsReadableProtect(memoryInfo.Protect);

					if (readable && regionSize >= 4096 && regionSize <= (256ull * 1024ull * 1024ull))
					{
						for (size_t offset = 0; offset < regionSize; offset += chunkSize)
						{
							const size_t readSize = (std::min)(chunkSize, regionSize - offset);
							SIZE_T bytesRead = 0;
							if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
								reinterpret_cast<LPCVOID>(regionBase + offset),
								buffer.data(), readSize, &bytesRead) || bytesRead == 0)
							{
								continue;
							}

							buffer.resize(bytesRead);
							for (const char* pattern : patterns)
							{
								const size_t patternLen = std::strlen(pattern);
								if (patternLen == 0 || bytesRead < patternLen)
									continue;

								for (size_t index = 0; index + patternLen <= bytesRead; ++index)
								{
									if (std::memcmp(buffer.data() + index, pattern, patternLen) != 0)
										continue;

									const std::string line = ExtractTriggerLineFromMemory(buffer, index);
									if (!LineHasAnyTriggerPattern(line))
										continue;

									const std::string resource = GuessResourceFromTriggerLine(line, resources);
									AddTriggerHit(resource, line, triggers, seenTriggers);
								}
							}
							buffer.resize(chunkSize);
						}
					}

					address = regionBase + regionSize;
				}
			}

			std::filesystem::path FindCacheTriggerScript()
			{
				const std::string exeDir = BrandPaths::GetExecutableDirectory();
				const std::vector<std::filesystem::path> candidates = {
					std::filesystem::path(exeDir) / "tools/trinity-cache-triggers/scan_triggers.py",
					std::filesystem::path(exeDir) / "../tools/trinity-cache-triggers/scan_triggers.py",
					std::filesystem::path(BrandPaths::GetDataRoot()) / "../tools/trinity-cache-triggers/scan_triggers.py",
					std::filesystem::path("E:/BH/c/FC2/tools/trinity-cache-triggers/scan_triggers.py"),
					std::filesystem::path("E:/PA/tools/trinity-cache-triggers/scan_triggers.py"),
				};

				for (const auto& candidate : candidates)
				{
					std::error_code ec;
					if (std::filesystem::exists(candidate, ec))
						return candidate;
				}

				return {};
			}

			std::string RunCommandCaptureOutput(const std::string& command)
			{
				std::string output;
				FILE* pipe = _popen(command.c_str(), "r");
				if (!pipe)
					return output;

				char buffer[4096];
				while (fgets(buffer, sizeof(buffer), pipe))
					output += buffer;

				_pclose(pipe);
				return output;
			}

			void MergeJsonTriggers(const std::string& jsonText, std::vector<DevTriggerEntry>& triggers,
				std::set<std::string>& seenTriggers)
			{
				if (jsonText.empty())
					return;

				try
				{
					const auto parsed = nlohmann::json::parse(jsonText);
					if (parsed.contains("error"))
						return;
					if (!parsed.contains("triggers") || !parsed["triggers"].is_array())
						return;

					for (const auto& item : parsed["triggers"])
					{
						std::string resource = item.value("resource", std::string("unknown"));
						std::string code = item.value("code", std::string());
						if (code.empty())
							code = item.value("file", std::string());
						AddTriggerHit(resource, code, triggers, seenTriggers);
					}
				}
				catch (...)
				{
				}
			}

			std::filesystem::path FindCacheScanTool()
			{
				const std::string exeDir = BrandPaths::GetExecutableDirectory();
				const std::vector<std::filesystem::path> candidates = {
					std::filesystem::path(exeDir) / "tools/TrinityCacheScan/TrinityCacheScan.exe",
					std::filesystem::path(exeDir) / "../tools/TrinityCacheScan/TrinityCacheScan.exe",
					std::filesystem::path(BrandPaths::GetDataRoot()) / "../tools/TrinityCacheScan/TrinityCacheScan.exe",
					std::filesystem::path("E:/BH/c/FC2/tools/TrinityCacheScan/bin/Release/net8.0/TrinityCacheScan.exe"),
					std::filesystem::path("E:/PA/tools/TrinityCacheScan/TrinityCacheScan.exe"),
				};

				for (const auto& candidate : candidates)
				{
					std::error_code ec;
					if (std::filesystem::exists(candidate, ec))
						return candidate;
				}

				return {};
			}

			struct BridgeDumpResult
			{
				bool attempted = false;
				bool success = false;
				size_t filesWritten = 0;
				std::string error;
			};

			BridgeDumpResult RunBridgeResourceDump(const std::filesystem::path& outputPath,
				bool includeStreamables, bool includeScripts, bool includeAllFiles,
				float progressStart, float progressEnd)
			{
				BridgeDumpResult result;
				if (!DevBridgeHttp::IsBridgeLive())
					return result;

				result.attempted = true;
				DevBridgeHttp::BeginDumpWait(outputPath);

				nlohmann::json payload = nlohmann::json::object();
				payload["includeStreamables"] = includeStreamables;
				payload["includeScripts"] = includeScripts;
				payload["includeAllFiles"] = includeAllFiles;
				DevBridgeHttp::QueueCommand("dump_resources", payload.dump());

				const auto started = std::chrono::steady_clock::now();
				const auto deadline = started + std::chrono::minutes(8);
				size_t lastWritten = 0;

				SetDumpProgress(progressStart, "Dumping via LoadResourceFile bridge");

				while (std::chrono::steady_clock::now() < deadline)
				{
					const size_t written = DevBridgeHttp::GetDumpFilesWritten();
					if (written != lastWritten)
					{
						lastWritten = written;
						const float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
						const float ratio = (std::min)(0.92f, progressStart + elapsed / 120.f * (progressEnd - progressStart));
						SetDumpProgress(ratio, "Dumping via LoadResourceFile bridge",
							std::to_string(written) + " files received", written);
					}

					if (DevBridgeHttp::IsDumpDone())
						break;

					size_t ignored = 0;
					if (DevBridgeHttp::WaitForDumpCompletion(500, ignored))
						break;
				}

				result.filesWritten = DevBridgeHttp::GetDumpFilesWritten();
				result.success = result.filesWritten > 0;
				if (!result.success)
					result.error = "Bridge dump returned no files. Ensure trinity-dev-bridge is running in-game.";

				SetDumpProgress(progressEnd, "Bridge dump finished",
					std::to_string(result.filesWritten) + " files received", result.filesWritten);
				return result;
			}

			struct DecryptedDumpResult
			{
				bool attempted = false;
				bool success = false;
				size_t filesWritten = 0;
				size_t streamWritten = 0;
				size_t scriptWritten = 0;
				size_t resources = 0;
				std::string error;
			};

			DecryptedDumpResult RunDecryptedCacheDump(const std::filesystem::path& cachePath,
				const std::filesystem::path& outputPath, bool includeStreamables, bool includeScripts, bool includeAllFiles)
			{
				DecryptedDumpResult result;
				const auto toolPath = FindCacheScanTool();
				if (toolPath.empty())
				{
					result.error = "TrinityCacheScan.exe not found";
					return result;
				}

				result.attempted = true;
				std::ostringstream command;
				command << "\"" << toolPath.string() << "\" dump \"" << cachePath.string()
					<< "\" \"" << outputPath.string() << "\"";
				if (includeAllFiles)
					command << " --all";
				else
				{
					if (includeStreamables)
						command << " --stream";
					if (includeScripts)
						command << " --scripts";
				}
				command << " 2>nul";

				const std::string output = RunCommandCaptureOutput(command.str());
				if (output.empty())
				{
					result.error = "Decrypted dump returned no output";
					return result;
				}

				try
				{
					const auto parsed = nlohmann::json::parse(output);
					if (parsed.contains("error"))
					{
						result.error = parsed["error"].get<std::string>();
						return result;
					}

					result.success = parsed.value("success", false);
					result.filesWritten = parsed.value("files_written", static_cast<size_t>(0));
					result.streamWritten = parsed.value("stream_written", static_cast<size_t>(0));
					result.scriptWritten = parsed.value("script_written", static_cast<size_t>(0));
					result.resources = parsed.value("resources", static_cast<size_t>(0));
				}
				catch (...)
				{
					result.error = "Failed to parse decrypted dump output";
				}

				return result;
			}

			void ScanDecryptedCacheForTriggers(const std::filesystem::path& cachePath,
				std::vector<DevTriggerEntry>& triggers, std::set<std::string>& seenTriggers)
			{
				const auto toolPath = FindCacheScanTool();
				if (!toolPath.empty())
				{
					std::ostringstream command;
					command << "\"" << toolPath.string() << "\" scan \"" << cachePath.string() << "\" 2>nul";
					const std::string output = RunCommandCaptureOutput(command.str());
					MergeJsonTriggers(output, triggers, seenTriggers);
					return;
				}

				const auto scriptPath = FindCacheTriggerScript();
				if (scriptPath.empty())
					return;

				std::ostringstream command;
				command << "python \"" << scriptPath.string() << "\" \"" << cachePath.string() << "\" 2>nul";
				const std::string output = RunCommandCaptureOutput(command.str());
				MergeJsonTriggers(output, triggers, seenTriggers);
			}

			void ScanBinaryForTriggers(const std::filesystem::path& filePath, const std::string& resourceName,
				std::vector<DevTriggerEntry>& triggers, std::set<std::string>& seenTriggers)
			{
				std::ifstream file(filePath, std::ios::binary);
				if (!file)
					return;

				std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				if (content.empty())
					return;

				static const char* patterns[] = {
					"TriggerServerEvent",
					"TriggerEvent",
					"TriggerLatentServerEvent"
				};

				for (const char* pattern : patterns)
				{
					const size_t patternLen = strlen(pattern);
					size_t pos = 0;
					while ((pos = content.find(pattern, pos)) != std::string::npos)
					{
						const size_t start = pos > 80 ? pos - 80 : 0;
						const size_t end = pos + 160 < content.size() ? pos + 160 : content.size();
						std::string snippet = TrimLine(content.substr(start, end - start));

						for (char& c : snippet)
						{
							if (c == '\r' || c == '\n' || c == '\0')
								c = ' ';
						}

						const std::string key = resourceName + '|' + snippet;
						if (seenTriggers.insert(key).second)
						{
							DevTriggerEntry trigger;
							trigger.resource = resourceName;
							trigger.code = snippet;
							triggers.push_back(std::move(trigger));
						}

						pos += patternLen;
					}
				}
			}

			std::vector<std::filesystem::path> GetScanRoots(const ServerSession& session)
			{
				std::vector<std::filesystem::path> roots;
				if (!session.cachePath.empty())
					roots.push_back(session.cachePath);

				const auto unconfirmed = session.cachePath / "unconfirmed";
				std::error_code ec;
				if (std::filesystem::exists(unconfirmed, ec))
					roots.push_back(unconfirmed);

				const auto dataRoot = GetFiveMDataRoot();
				const auto filesCache = dataRoot / "cache" / "files";
				if (std::filesystem::exists(filesCache, ec))
					roots.push_back(filesCache);

				return roots;
			}

			void ScanCacheEntry(const std::filesystem::path& filePath, ServerScanResult& result,
				std::set<std::string>& seenTriggers, bool scanTriggers)
			{
				if (!IsCacheBlobFile(filePath))
					return;

				++result.totalFileCount;
				++result.streamFileCount;
				if (scanTriggers)
					ScanBinaryForTriggers(filePath, "cached", result.triggers, seenTriggers);
			}

			bool IsStreamExtension(const std::filesystem::path& path)
			{
				static const std::set<std::string> streamExtensions = {
					".yft", ".ytd", ".ydr", ".ybn", ".ymap", ".ydd", ".ycd", ".ytyp", ".ymf",
					".awc", ".meta", ".xml", ".rpf", ".gfx", ".cwgv", ".svg", ".dds",
					".png", ".jpg", ".jpeg", ".webp", ".ogg", ".wav", ".mp3", ".dat", ".cache"
				};

				const std::string ext = ToLower(path.extension().string());
				if (streamExtensions.count(ext))
					return true;

				const std::string full = ToLower(path.generic_string());
				return full.find("/stream/") != std::string::npos || full.find("\\stream\\") != std::string::npos;
			}

			bool IsScriptExtension(const std::filesystem::path& path)
			{
				static const std::set<std::string> scriptExtensions = {
					".lua", ".js", ".ts", ".json", ".html", ".htm", ".css", ".xml", ".txt", ".md"
				};

				const std::string ext = ToLower(path.extension().string());
				return scriptExtensions.count(ext) > 0;
			}

			bool IsManifestFile(const std::filesystem::path& path)
			{
				const std::string name = ToLower(path.filename().string());
				return name == "fxmanifest.lua" || name == "__resource.lua";
			}

			bool ShouldIncludeFile(const std::filesystem::path& path, bool includeStreamables, bool includeScripts, bool includeAllFiles)
			{
				if (includeAllFiles)
					return !IsLevelDbArtifact(path);

				if (IsManifestFile(path))
					return true;

				if (IsCacheBlobFile(path) && includeStreamables)
					return true;

				if (includeScripts && IsScriptExtension(path))
					return true;

				if (includeStreamables && IsStreamExtension(path))
					return true;

				return false;
			}

			void ScanFileForTriggers(const std::filesystem::path& filePath, const std::string& resourceName,
				std::vector<DevTriggerEntry>& triggers, std::set<std::string>& seenTriggers)
			{
				if (!IsScriptExtension(filePath) && filePath.extension() != ".lua")
					return;

				std::ifstream file(filePath, std::ios::binary);
				if (!file)
					return;

				std::string line;
				while (std::getline(file, line))
				{
					if (!LineHasTrigger(line))
						continue;

					const std::string trimmed = TrimLine(line);
					const std::string key = resourceName + '|' + trimmed;
					if (!seenTriggers.insert(key).second)
						continue;

					DevTriggerEntry trigger;
					trigger.resource = resourceName;
					trigger.code = trimmed;
					triggers.push_back(std::move(trigger));
				}
			}

			std::vector<std::filesystem::path> GetServerCacheRoots()
			{
				std::vector<std::filesystem::path> roots;
				const auto dataRoot = GetFiveMDataRoot();
				if (dataRoot.empty())
					return roots;

				roots.push_back(dataRoot / "server-cache-priv");
				roots.push_back(dataRoot / "server-cache");
				return roots;
			}

			RuntimeHarvestResult HarvestRuntimeClientScriptsInternal()
			{
				RuntimeHarvestResult harvest;
				const ServerSession session = GetActiveServerSession();
				const bool attached = FrameWork::Memory::AttachedProcessHandle != nullptr;

				if (session.cachePath.empty() && !attached)
				{
					harvest.message = "Join a server and wait for Trinity to attach.";
					return harvest;
				}

				const auto safeLabel = SanitizePathLabel(
					session.label.empty() ? (attached ? "FiveM-live" : "FiveM-cache") : session.label);
				harvest.outputPath = std::filesystem::path(BrandPaths::GetDataRoot()) / "lua" / "runtime" / safeLabel;

				std::error_code ec;
				std::filesystem::create_directories(harvest.outputPath, ec);

				std::vector<DevResourceEntry> resources;
				if (!session.infoPath.empty())
					ParseResourcesFromServerJson(ReadTextFileIfExists(session.infoPath), resources);

				size_t filesWritten = 0;

				if (attached)
				{
					const MemoryDumpResult memoryDump = DumpScriptsFromProcessMemory(
						harvest.outputPath, resources, false, true, false, 0.f, 1.f);
					filesWritten += memoryDump.filesWritten;
					harvest.resourceCount = memoryDump.resources;
				}

				if (!session.cachePath.empty())
				{
					const DecryptedDumpResult cacheDump = RunDecryptedCacheDump(
						session.cachePath, harvest.outputPath, false, true, false);
					filesWritten += cacheDump.scriptWritten;
					if (cacheDump.resources > harvest.resourceCount)
						harvest.resourceCount = cacheDump.resources;
				}

				std::set<std::string> names;
				if (std::filesystem::exists(harvest.outputPath, ec))
				{
					for (std::filesystem::recursive_directory_iterator it(harvest.outputPath, std::filesystem::directory_options::skip_permission_denied, ec);
						it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
					{
						if (ec)
						{
							ec.clear();
							continue;
						}

						if (!it->is_regular_file(ec))
							continue;

						const auto relative = std::filesystem::relative(it->path(), harvest.outputPath, ec).generic_string();
						if (IsServerOnlyPath(ToLower(relative)))
							continue;
						if (it->path().extension() != ".lua" && it->path().extension() != ".js" && it->path().extension() != ".html")
							continue;

						auto parent = it->path().parent_path();
						while (parent != harvest.outputPath && parent.has_parent_path())
						{
							if (parent.parent_path() == harvest.outputPath)
							{
								names.insert(parent.filename().string());
								break;
							}
							parent = parent.parent_path();
						}
					}
				}

				harvest.resourceNames.assign(names.begin(), names.end());
				if (harvest.resourceCount < harvest.resourceNames.size())
					harvest.resourceCount = harvest.resourceNames.size();

				harvest.clientFiles = filesWritten;
				harvest.success = filesWritten > 0;
				harvest.message = harvest.success
					? ("Harvested " + std::to_string(filesWritten) + " client scripts from server memory/cache.")
					: "No client scripts found yet — wait for resources to finish loading.";
				return harvest;
			}
		}

		std::filesystem::path GetFiveMDataRoot()
		{
			char localAppData[MAX_PATH]{};
			if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, localAppData)))
				return {};

			return std::filesystem::path(localAppData) / "FiveM" / "FiveM.app" / "data";
		}

		std::vector<ServerSession> ListServerSessions()
		{
			std::vector<ServerSession> sessions;
			const auto dataRoot = GetFiveMDataRoot();
			if (dataRoot.empty())
				return sessions;

			const auto serversDir = dataRoot / "cache" / "servers";
			const auto cachePrivRoot = dataRoot / "server-cache-priv";
			std::error_code ec;
			if (!std::filesystem::exists(serversDir, ec))
				return sessions;

			for (const auto& entry : std::filesystem::directory_iterator(serversDir, ec))
			{
				if (ec || !entry.is_regular_file(ec))
					continue;

				if (ToLower(entry.path().extension().string()) != ".json")
					continue;

				const std::string json = ReadTextFileIfExists(entry.path());
				if (json.empty())
					continue;

				ServerSession session;
				session.infoPath = entry.path();
				session.cachePath = cachePrivRoot;
				session.label = ExtractJsonStringValue(json, "sv_projectName");
				if (session.label.empty())
					session.label = entry.path().stem().string();
				session.lastWriteUnix = GetPathLastWriteUnix(entry.path());
				sessions.push_back(std::move(session));
			}

			std::sort(sessions.begin(), sessions.end(), [](const ServerSession& a, const ServerSession& b)
			{
				return a.lastWriteUnix > b.lastWriteUnix;
			});

			return sessions;
		}

		ServerSession GetActiveServerSession()
		{
			const auto sessions = ListServerSessions();
			if (!sessions.empty())
				return sessions.front();

			ServerSession fallback;
			const auto cachePrivRoot = GetFiveMDataRoot() / "server-cache-priv";
			std::error_code ec;
			if (std::filesystem::exists(cachePrivRoot, ec))
			{
				fallback.cachePath = cachePrivRoot;
				fallback.label = "FiveM cache";
				fallback.lastWriteUnix = GetPathLastWriteUnix(cachePrivRoot);
			}
			else
			{
				fallback.label = "none";
			}
			return fallback;
		}

		ServerScanResult ScanActiveServer()
		{
			ServerScanResult result;
			result.session = GetActiveServerSession();
			if (result.session.cachePath.empty())
				return result;

			if (!result.session.infoPath.empty())
			{
				const std::string json = ReadTextFileIfExists(result.session.infoPath);
				ParseResourcesFromServerJson(json, result.resources);
				const std::string projectName = ExtractJsonStringValue(json, "sv_projectName");
				if (!projectName.empty())
					result.session.label = projectName;
			}

			std::set<std::string> seenTriggers;

			std::error_code ec;
			if (std::filesystem::exists(result.session.cachePath, ec))
			{
				for (const auto& entry : std::filesystem::directory_iterator(result.session.cachePath, ec))
				{
					if (ec)
					{
						ec.clear();
						continue;
					}

					if (entry.is_regular_file(ec))
					{
						ScanCacheEntry(entry.path(), result, seenTriggers, false);
						continue;
					}

					if (!entry.is_directory(ec))
						continue;

					const auto dirName = entry.path().filename().string();
					if (dirName == "db" || dirName == "unconfirmed")
						continue;

					for (std::filesystem::recursive_directory_iterator it(entry.path(), std::filesystem::directory_options::skip_permission_denied, ec);
						it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
					{
						if (ec)
						{
							ec.clear();
							continue;
						}

						if (!it->is_regular_file(ec))
							continue;

						if (IsCacheBlobFile(it->path()))
						{
							ScanCacheEntry(it->path(), result, seenTriggers, false);
						}
						else if (IsScriptExtension(it->path()))
						{
							++result.totalFileCount;
							++result.scriptFileCount;
							std::string resourceName = it->path().parent_path().filename().string();
							ScanFileForTriggers(it->path(), resourceName, result.triggers, seenTriggers);
						}
						else if (IsStreamExtension(it->path()))
						{
							++result.totalFileCount;
							++result.streamFileCount;
						}
					}
				}

				const auto unconfirmed = result.session.cachePath / "unconfirmed";
				if (std::filesystem::exists(unconfirmed, ec))
				{
					for (const auto& entry : std::filesystem::directory_iterator(unconfirmed, ec))
					{
						if (ec || !entry.is_regular_file(ec))
							continue;

						ScanCacheEntry(entry.path(), result, seenTriggers, false);
					}
				}
			}

			ScanDecryptedCacheForTriggers(result.session.cachePath, result.triggers, seenTriggers);
			ScanProcessMemoryForTriggers(result.resources, result.triggers, seenTriggers);

			for (auto& resource : result.resources)
			{
				int count = 0;
				for (const auto& trigger : result.triggers)
				{
					if (trigger.resource == resource.name)
						++count;
				}
				resource.eventCount = count;
			}

			std::sort(result.resources.begin(), result.resources.end(),
				[](const DevResourceEntry& a, const DevResourceEntry& b) { return a.name < b.name; });

			return result;
		}

		ServerDumpResult DumpActiveServer(const std::filesystem::path& outputRoot, bool includeStreamables, bool includeScripts, bool includeAllFiles)
		{
			BeginDumpProgress();
			ServerDumpResult result;
			const ServerSession session = GetActiveServerSession();
			const bool attached = FrameWork::Memory::AttachedProcessHandle != nullptr;

			if (session.cachePath.empty() && !attached)
			{
				result.message = "Join a FiveM server and wait for Trinity to attach, then retry.";
				FinishDumpProgress(0);
				return result;
			}

			SetDumpProgress(0.05f, "Preparing output folder");

			const auto safeLabel = SanitizePathLabel(
				session.label.empty() ? (attached ? "FiveM-live" : "FiveM-cache") : session.label);
			const auto outputPath = outputRoot / safeLabel;
			std::error_code ec;
			std::filesystem::create_directories(outputRoot, ec);
			std::filesystem::create_directories(outputPath, ec);

			if (!session.infoPath.empty())
			{
				const std::string json = ReadTextFileIfExists(session.infoPath);
				if (!json.empty())
				{
					std::ofstream resourcesOut(outputPath / "resources.json", std::ios::binary | std::ios::trunc);
					if (resourcesOut)
						resourcesOut << json;
				}
			}

			std::vector<DevResourceEntry> resources;
			if (!session.infoPath.empty())
				ParseResourcesFromServerJson(ReadTextFileIfExists(session.infoPath), resources);

			BridgeDumpResult bridgeDump;
			if (DevBridgeHttp::IsBridgeLive())
			{
				SetDumpProgress(0.08f, "Starting Susano-style bridge dump");
				bridgeDump = RunBridgeResourceDump(
					outputPath, includeStreamables, includeScripts, includeAllFiles, 0.10f, 0.82f);
				if (bridgeDump.success)
				{
					result.filesCopied = bridgeDump.filesWritten;
					result.scriptCopied = bridgeDump.filesWritten;
					result.success = true;
				}
			}

			DecryptedDumpResult cacheDump;
			if (!session.cachePath.empty())
			{
				SetDumpProgress(0.12f, "Decrypting local cache");
				cacheDump = RunDecryptedCacheDump(
					session.cachePath, outputPath, includeStreamables, includeScripts, includeAllFiles);
				SetDumpProgress(0.40f, "Cache decrypt finished", std::to_string(cacheDump.filesWritten) + " cache files",
					cacheDump.filesWritten);
				if (cacheDump.success)
				{
					result.filesCopied += cacheDump.filesWritten;
					result.streamCopied += cacheDump.streamWritten;
					result.scriptCopied += cacheDump.scriptWritten;
					result.success = true;
				}
			}

			if (attached)
			{
				const MemoryDumpResult memoryDump = DumpScriptsFromProcessMemory(
					outputPath, resources, includeStreamables, true, includeAllFiles, 0.42f, 0.95f);
				if (memoryDump.filesWritten > 0)
				{
					result.filesCopied += memoryDump.filesWritten;
					result.scriptCopied += memoryDump.filesWritten;
					result.success = true;
				}
			}

			SetDumpProgress(0.98f, "Writing dump info");
			const auto infoPath = outputPath / "_trinity_dump_info.txt";
			std::ofstream info(infoPath, std::ios::trunc);
			if (info)
			{
				info << "Server label: " << session.label << '\n';
				info << "Cache path: " << session.cachePath.string() << '\n';
				info << "Attached to FiveM: " << (attached ? "yes" : "no") << '\n';
				info << "Bridge dump: " << (bridgeDump.success ? "yes" : "no") << '\n';
				if (!bridgeDump.error.empty())
					info << "Bridge dump note: " << bridgeDump.error << '\n';
				info << "Cache decrypt dump: " << (cacheDump.success ? "yes" : "no") << '\n';
				if (!cacheDump.error.empty())
					info << "Cache dump note: " << cacheDump.error << '\n';
				info << "Resources: " << cacheDump.resources << '\n';
				info << "Files copied: " << result.filesCopied << '\n';
				info << "Stream files: " << result.streamCopied << '\n';
				info << "Script files: " << result.scriptCopied << '\n';
			}

			result.outputPath = outputPath;
			if (bridgeDump.success)
			{
				result.message = "Dumped " + std::to_string(result.filesCopied)
					+ " files via LoadResourceFile (Susano-style) to resource folders.";
			}
			else if (cacheDump.success)
			{
				result.message = "Dumped " + std::to_string(result.filesCopied) + " files across "
					+ std::to_string(cacheDump.resources) + " resources from local cache.";
			}
			else if (result.success)
			{
				result.message = "Dumped " + std::to_string(result.scriptCopied)
					+ " client-side files from FiveM memory/cache into resource folders.";
			}
			else if (!attached && !DevBridgeHttp::IsBridgeLive())
			{
				result.message = "Could not dump. Join a server, wait for Trinity to attach, then retry.";
			}
			else if (DevBridgeHttp::IsBridgeLive() && bridgeDump.attempted)
			{
				result.message = "Bridge dump found no files. Stay in-game until resources finish loading, then retry.";
			}
			else if (cacheDump.attempted && !cacheDump.error.empty())
			{
				result.message = "Cache decrypt failed and memory dump found no scripts. Stay in-game until resources load, then retry.";
			}
			else
			{
				result.message = "No client scripts extracted. Join the server, wait for resources to finish loading, then dump again.";
			}

			FinishDumpProgress(result.filesCopied);
			return result;
		}

		DumpProgressState GetDumpProgress()
		{
			DumpProgressState state;
			std::lock_guard<std::mutex> lock(g_dumpProgressMutex);
			state.active = g_dumpProgressActive.load();
			state.progress = g_dumpProgressValue.load();
			state.filesWritten = g_dumpProgressFiles.load();
			state.phase = g_dumpProgressPhase;
			state.detail = g_dumpProgressDetail;
			return state;
		}

		RuntimeHarvestResult HarvestRuntimeClientScripts()
		{
			return HarvestRuntimeClientScriptsInternal();
		}
	}
}

#else

namespace Cheat
{
	namespace ServerDump
	{
		std::filesystem::path GetFiveMDataRoot() { return {}; }
		std::vector<ServerSession> ListServerSessions() { return {}; }
		ServerSession GetActiveServerSession() { return {}; }
		ServerScanResult ScanActiveServer() { return {}; }
		ServerDumpResult DumpActiveServer(const std::filesystem::path&, bool, bool, bool) { return {}; }
		DumpProgressState GetDumpProgress() { return {}; }
		RuntimeHarvestResult HarvestRuntimeClientScripts() { return {}; }
	}
}

#endif
