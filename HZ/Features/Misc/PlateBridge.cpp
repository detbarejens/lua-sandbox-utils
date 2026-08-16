#include "PlateBridge.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace Cheat
{
	namespace PlateBridge
	{
		namespace
		{
			constexpr int kBridgePort = 27272;

			std::mutex g_plateMutex;
			std::string g_pendingPlate;
			std::atomic<bool> g_running{ false };
			std::thread g_serverThread;

			std::string JsonEscape(const std::string& value)
			{
				std::string escaped;
				escaped.reserve(value.size() + 8);
				for (char c : value)
				{
					if (c == '\\' || c == '"')
						escaped.push_back('\\');
					if (static_cast<unsigned char>(c) >= 0x20 && c != '\x7f')
						escaped.push_back(c);
				}
				return escaped;
			}

			void SendHttpResponse(SOCKET client, int statusCode, const char* statusText, const std::string& body)
			{
				std::string response =
					"HTTP/1.1 " + std::to_string(statusCode) + ' ' + statusText +
					"\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: " +
					std::to_string(body.size()) + "\r\n\r\n" + body;

				send(client, response.c_str(), static_cast<int>(response.size()), 0);
			}

			void HandleClient(SOCKET client)
			{
				char buffer[1024] = {};
				const int received = recv(client, buffer, sizeof(buffer) - 1, 0);
				if (received <= 0)
					return;

				std::string request(buffer, static_cast<size_t>(received));
				if (request.find("GET /plate") == std::string::npos)
				{
					SendHttpResponse(client, 404, "Not Found", "{\"error\":\"not found\"}");
					return;
				}

				std::string plate;
				{
					std::lock_guard<std::mutex> lock(g_plateMutex);
					plate = g_pendingPlate;
				}

				const std::string body = std::string("{\"plate\":\"") + JsonEscape(plate) + "\"}";
				SendHttpResponse(client, 200, "OK", body);
			}

			void ServerLoop()
			{
				WSADATA wsaData{};
				if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
					return;

				SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (listenSocket == INVALID_SOCKET)
				{
					WSACleanup();
					return;
				}

				BOOL reuse = TRUE;
				setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

				sockaddr_in address{};
				address.sin_family = AF_INET;
				address.sin_port = htons(kBridgePort);
				inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

				if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
					listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
				{
					closesocket(listenSocket);
					WSACleanup();
					return;
				}

				while (g_running.load(std::memory_order_relaxed))
				{
					fd_set readSet;
					FD_ZERO(&readSet);
					FD_SET(listenSocket, &readSet);

					timeval timeout{};
					timeout.tv_sec = 0;
					timeout.tv_usec = 250000;

					const int ready = select(0, &readSet, nullptr, nullptr, &timeout);
					if (ready <= 0)
						continue;

					sockaddr_in clientAddress{};
					int clientLength = sizeof(clientAddress);
					SOCKET client = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientLength);
					if (client == INVALID_SOCKET)
						continue;

					HandleClient(client);
					closesocket(client);
				}

				closesocket(listenSocket);
				WSACleanup();
			}
		}

		void Start()
		{
			if (g_running.exchange(true))
				return;

			if (g_serverThread.joinable())
				g_serverThread.join();

			g_serverThread = std::thread(ServerLoop);
		}

		void Stop()
		{
			if (!g_running.exchange(false))
				return;

			if (g_serverThread.joinable())
				g_serverThread.join();
		}

		void QueuePlate(const std::string& plate)
		{
			std::lock_guard<std::mutex> lock(g_plateMutex);
			g_pendingPlate = plate;
		}

		bool IsRunning()
		{
			return g_running.load(std::memory_order_relaxed);
		}
	}
}

#else

namespace Cheat
{
	namespace PlateBridge
	{
		void Start() {}
		void Stop() {}
		void QueuePlate(const std::string&) {}
		bool IsRunning() { return false; }
	}
}

#endif
