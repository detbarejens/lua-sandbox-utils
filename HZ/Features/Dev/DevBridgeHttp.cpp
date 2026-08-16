#include "DevBridgeHttp.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "../../json.hpp"

#include <WinSock2.h>
#include <Windows.h>
#include <WS2tcpip.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace Cheat
{
	namespace DevBridgeHttp
	{
		namespace
		{
			constexpr unsigned short kPort = 27273;

			std::mutex g_mutex;
			std::condition_variable g_dumpCv;
			std::vector<nlohmann::json> g_commands;
			std::chrono::steady_clock::time_point g_lastPush{};
			std::string g_lastPushBody;
			std::filesystem::path g_dumpOutputRoot;
			std::atomic<bool> g_dumpPending{ false };
			std::atomic<bool> g_dumpDone{ false };
			std::atomic<size_t> g_dumpFilesWritten{ 0 };
			std::atomic<bool> g_running{ false };
			SOCKET g_listenSocket = INVALID_SOCKET;
			std::thread g_serverThread;

			std::string SanitizeLabel(std::string label)
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
					label = "resource";
				return label;
			}

			std::string HttpResponse(int code, const std::string& status, const std::string& contentType, const std::string& body)
			{
				std::ostringstream ss;
				ss << "HTTP/1.1 " << code << ' ' << status << "\r\n";
				ss << "Content-Type: " << contentType << "\r\n";
				ss << "Access-Control-Allow-Origin: *\r\n";
				ss << "Content-Length: " << body.size() << "\r\n";
				ss << "Connection: close\r\n\r\n";
				ss << body;
				return ss.str();
			}

			bool RecvAll(SOCKET socket, char* buffer, int length)
			{
				int received = 0;
				while (received < length)
				{
					const int chunk = recv(socket, buffer + received, length - received, 0);
					if (chunk <= 0)
						return false;
					received += chunk;
				}
				return true;
			}

			size_t ParseContentLength(const std::string& headers)
			{
				const std::string key = "Content-Length:";
				const auto pos = headers.find(key);
				if (pos == std::string::npos)
					return 0;

				size_t value = 0;
				const auto lineEnd = headers.find("\r\n", pos);
				const std::string line = headers.substr(pos + key.size(), lineEnd == std::string::npos ? std::string::npos : lineEnd - pos - key.size());
				std::istringstream ss(line);
				ss >> value;
				return value;
			}

			std::string ReadRequest(SOCKET client)
			{
				std::string data;
				char buffer[8192];
				for (;;)
				{
					const int read = recv(client, buffer, sizeof(buffer), 0);
					if (read <= 0)
						break;
					data.append(buffer, buffer + read);

					const auto headerEnd = data.find("\r\n\r\n");
					if (headerEnd == std::string::npos)
					{
						if (data.size() > 16 * 1024 * 1024)
							break;
						continue;
					}

					const size_t contentLength = ParseContentLength(data.substr(0, headerEnd));
					const size_t bodyStart = headerEnd + 4;
					const size_t needed = bodyStart + contentLength;
					if (data.size() >= needed)
						break;

					if (needed > 64 * 1024 * 1024)
						break;
				}
				return data;
			}

			size_t WriteDumpFiles(const nlohmann::json& files)
			{
				if (!files.is_array())
					return 0;

				std::filesystem::path outputRoot;
				{
					std::lock_guard<std::mutex> lock(g_mutex);
					outputRoot = g_dumpOutputRoot;
				}
				if (outputRoot.empty())
					return 0;

				size_t written = 0;
				std::set<std::string> seen;
				for (const auto& item : files)
				{
					if (!item.is_object())
						continue;

					const std::string resource = SanitizeLabel(item.value("resource", std::string("unknown")));
					const std::string relative = item.value("path", std::string());
					const std::string content = item.value("content", std::string());
					if (relative.empty() || content.empty())
						continue;

					const std::string key = resource + "|" + relative;
					if (!seen.insert(key).second)
						continue;

					std::filesystem::path target = outputRoot / resource;
					for (const auto part : std::filesystem::path(relative))
						target /= part;

					std::error_code ec;
					std::filesystem::create_directories(target.parent_path(), ec);
					std::ofstream out(target, std::ios::binary | std::ios::trunc);
					if (!out)
						continue;

					out << content;
					++written;
				}

				return written;
			}

			void HandleClient(SOCKET client)
			{
				const std::string request = ReadRequest(client);
				if (request.empty())
				{
					closesocket(client);
					return;
				}

				const auto lineEnd = request.find("\r\n");
				const std::string line = lineEnd == std::string::npos ? request : request.substr(0, lineEnd);
				const auto bodyStart = request.find("\r\n\r\n");
				const std::string body = bodyStart == std::string::npos ? std::string() : request.substr(bodyStart + 4);

				std::string method;
				std::string path;
				{
					std::istringstream ss(line);
					ss >> method >> path;
				}

				std::string responseBody = "[]";
				std::string response = HttpResponse(200, "OK", "application/json", responseBody);

				if (method == "GET" && path == "/api/commands")
				{
					nlohmann::json commands = nlohmann::json::array();
					{
						std::lock_guard<std::mutex> lock(g_mutex);
						for (const auto& command : g_commands)
							commands.push_back(command);
						g_commands.clear();
					}
					responseBody = commands.dump();
					response = HttpResponse(200, "OK", "application/json", responseBody);
				}
				else if (method == "POST" && path == "/api/push")
				{
					{
						std::lock_guard<std::mutex> lock(g_mutex);
						g_lastPush = std::chrono::steady_clock::now();
						g_lastPushBody = body;
					}
					responseBody = "{\"ok\":true}";
					response = HttpResponse(200, "OK", "application/json", responseBody);
				}
				else if (method == "POST" && path == "/api/dump_receive")
				{
					try
					{
						const auto parsed = nlohmann::json::parse(body);
						const size_t written = WriteDumpFiles(parsed.value("files", nlohmann::json::array()));
						g_dumpFilesWritten.fetch_add(written);
						if (parsed.value("done", false))
						{
							g_dumpDone.store(true);
							g_dumpPending.store(false);
							g_dumpCv.notify_all();
						}
						responseBody = nlohmann::json({ {"written", written} }).dump();
					}
					catch (...)
					{
						responseBody = "{\"error\":\"invalid json\"}";
					}
					response = HttpResponse(200, "OK", "application/json", responseBody);
				}
				else if (method == "OPTIONS")
				{
					response = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS\r\nAccess-Control-Allow-Headers: Content-Type\r\nConnection: close\r\n\r\n";
				}
				else
				{
					responseBody = "{\"error\":\"not found\"}";
					response = HttpResponse(404, "Not Found", "application/json", responseBody);
				}

				send(client, response.c_str(), static_cast<int>(response.size()), 0);
				closesocket(client);
			}

			void ServerLoop()
			{
				while (g_running.load())
				{
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(g_listenSocket, &readSet);

					timeval timeout{};
					timeout.tv_sec = 1;
					timeout.tv_usec = 0;

					const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
					if (!g_running.load())
						break;
					if (ready <= 0)
						continue;

					const SOCKET client = accept(g_listenSocket, nullptr, nullptr);
					if (client == INVALID_SOCKET)
						continue;

					HandleClient(client);
				}
			}
		}

		void Start()
		{
			if (g_running.load())
				return;

			WSADATA wsaData{};
			if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
				return;

			g_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (g_listenSocket == INVALID_SOCKET)
				return;

			BOOL reuse = TRUE;
			setsockopt(g_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

			sockaddr_in address{};
			address.sin_family = AF_INET;
			address.sin_port = htons(kPort);
			inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

			if (bind(g_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
				listen(g_listenSocket, SOMAXCONN) == SOCKET_ERROR)
			{
				closesocket(g_listenSocket);
				g_listenSocket = INVALID_SOCKET;
				return;
			}

			g_running.store(true);
			g_serverThread = std::thread(ServerLoop);
		}

		void Stop()
		{
			g_running.store(false);
			if (g_listenSocket != INVALID_SOCKET)
			{
				closesocket(g_listenSocket);
				g_listenSocket = INVALID_SOCKET;
			}
			if (g_serverThread.joinable())
				g_serverThread.join();
			WSACleanup();
		}

		bool IsBridgeLive()
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			if (g_lastPush.time_since_epoch().count() == 0)
				return false;
			return std::chrono::steady_clock::now() - g_lastPush < std::chrono::seconds(6);
		}

		std::string GetLastPushBody()
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			return g_lastPushBody;
		}

		void QueueCommand(const std::string& type, const std::string& payloadJson)
		{
			nlohmann::json payload = nlohmann::json::object();
			try
			{
				if (!payloadJson.empty())
					payload = nlohmann::json::parse(payloadJson);
			}
			catch (...)
			{
			}

			std::lock_guard<std::mutex> lock(g_mutex);
			g_commands.push_back({ {"type", type}, {"payload", payload} });
		}

		void BeginDumpWait(const std::filesystem::path& outputRoot)
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			g_dumpOutputRoot = outputRoot;
			g_dumpPending.store(true);
			g_dumpDone.store(false);
			g_dumpFilesWritten.store(0);
		}

		bool WaitForDumpCompletion(unsigned timeoutMs, size_t& filesWritten)
		{
			std::unique_lock<std::mutex> lock(g_mutex);
			const bool finished = g_dumpCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), []()
			{
				return g_dumpDone.load();
			});

			filesWritten = g_dumpFilesWritten.load();
			if (g_dumpDone.load())
				g_dumpPending.store(false);
			return finished || g_dumpDone.load();
		}

		bool IsDumpPending()
		{
			return g_dumpPending.load();
		}

		bool IsDumpDone()
		{
			return g_dumpDone.load();
		}

		size_t GetDumpFilesWritten()
		{
			return g_dumpFilesWritten.load();
		}

		void MergePushState(const std::string& jsonBody, DevBridgeState& state)
		{
			if (jsonBody.empty())
				return;

			try
			{
				const auto parsed = nlohmann::json::parse(jsonBody);
				if (parsed.contains("settings") && parsed["settings"].is_object())
				{
					const auto& settings = parsed["settings"];
					if (settings.contains("logCts")) state.logCts = settings["logCts"].get<bool>();
					if (settings.contains("logStc")) state.logStc = settings["logStc"].get<bool>();
					if (settings.contains("logNui")) state.logNui = settings["logNui"].get<bool>();
				}

				if (parsed.contains("resources") && parsed["resources"].is_array())
				{
					state.resources.clear();
					for (const auto& item : parsed["resources"])
					{
						DevResourceEntry entry;
						entry.name = item.value("name", std::string());
						entry.state = item.value("state", std::string());
						entry.threadCount = item.value("threadCount", 0);
						entry.eventCount = item.value("eventCount", 0);
						if (!entry.name.empty())
							state.resources.push_back(std::move(entry));
					}
				}

				if (parsed.contains("triggers") && parsed["triggers"].is_array())
				{
					state.triggers.clear();
					for (const auto& item : parsed["triggers"])
					{
						DevTriggerEntry trigger;
						trigger.resource = item.value("resource", std::string());
						trigger.code = item.value("code", std::string());
						if (!trigger.code.empty())
							state.triggers.push_back(std::move(trigger));
					}
				}

				if (parsed.contains("blockedEvents") && parsed["blockedEvents"].is_array())
				{
					state.blockedEvents.clear();
					for (const auto& item : parsed["blockedEvents"])
					{
						if (item.is_string())
							state.blockedEvents.push_back(item.get<std::string>());
					}
				}

				if (parsed.contains("ctsEvents") && parsed["ctsEvents"].is_array())
				{
					state.ctsEvents.clear();
					for (const auto& item : parsed["ctsEvents"])
					{
						DevEventEntry entry;
						entry.timestamp = item.value("timestamp", std::string());
						entry.event = item.value("event", std::string());
						entry.data = item.value("data", std::string());
						state.ctsEvents.push_back(std::move(entry));
					}
				}

				if (parsed.contains("stcEvents") && parsed["stcEvents"].is_array())
				{
					state.stcEvents.clear();
					for (const auto& item : parsed["stcEvents"])
					{
						DevEventEntry entry;
						entry.timestamp = item.value("timestamp", std::string());
						entry.event = item.value("event", std::string());
						entry.data = item.value("data", std::string());
						state.stcEvents.push_back(std::move(entry));
					}
				}

				if (parsed.contains("nuiEvents") && parsed["nuiEvents"].is_array())
				{
					state.nuiEvents.clear();
					for (const auto& item : parsed["nuiEvents"])
					{
						DevEventEntry entry;
						entry.timestamp = item.value("timestamp", std::string());
						entry.event = item.value("event", std::string());
						entry.data = item.value("data", std::string());
						state.nuiEvents.push_back(std::move(entry));
					}
				}

				if (parsed.contains("threads") && parsed["threads"].is_array())
				{
					state.threads.clear();
					for (const auto& item : parsed["threads"])
					{
						DevThreadEntry thread;
						thread.name = item.value("name", std::string());
						thread.status = item.value("status", std::string());
						state.threads.push_back(std::move(thread));
					}
				}
			}
			catch (...)
			{
			}
		}
	}
}

#else

namespace Cheat
{
	namespace DevBridgeHttp
	{
		void Start() {}
		void Stop() {}
		bool IsBridgeLive() { return false; }
		std::string GetLastPushBody() { return {}; }
		void QueueCommand(const std::string&, const std::string&) {}
		void BeginDumpWait(const std::filesystem::path&) {}
		bool WaitForDumpCompletion(unsigned, size_t&) { return false; }
		bool IsDumpPending() { return false; }
		bool IsDumpDone() { return false; }
		size_t GetDumpFilesWritten() { return 0; }
		void MergePushState(const std::string&, DevBridgeState&) {}
	}
}

#endif
