#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <WinSock2.h>
#include <Windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <functional>

#pragma comment(lib, "ws2_32.lib")

namespace WebControl
{
	class CheatClient
	{
	public:
		CheatClient();
		~CheatClient();

		bool Connect(const std::string& host, int port, const std::string& authToken, const std::string& hwidHash = "");
		void Disconnect();
		bool IsConnected() const { return bConnected; }

		// Send JSON message to backend
		bool Send(const std::string& json);

		// Set callback for incoming messages
		void OnMessage(std::function<void(const std::string&)> callback);

		// Auto-reconnect settings
		void SetAutoReconnect(bool enable, int intervalMs = 5000);

	private:
		std::string host;
		int port;
		std::string authToken;
		std::string hwidHash;
		SOCKET socket_;
		bool bConnected;
		bool bRunning;
		bool bAutoReconnect;
		int reconnectInterval;

		std::thread recvThread;
		std::function<void(const std::string&)> messageCallback;

		bool PerformHandshake();
		void ReceiveLoop();
		std::vector<uint8_t> CreateFrame(const std::string& data, uint8_t opcode = 0x1);
	};

	// Sync cheat options to JSON for backend communication
	void SyncOptionsToJson(std::string& outJson);
	bool ApplyOptionFromJson(const std::string& json);

	// Generate HWID (SHA256 of volume serial + computer name)
	std::string GenerateHWID();
}
