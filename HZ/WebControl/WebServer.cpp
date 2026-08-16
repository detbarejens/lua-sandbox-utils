#include "WebServer.hpp"
#include "../Definations/Cheat.hpp"
#include "../Security/XorStr.hpp"
#include "../Utils/DebugLog.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <bcrypt.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "bcrypt.lib")

namespace WebControl
{
#define WS_LOG(fmt, ...) MELLO_DBG("[WS] " fmt, ##__VA_ARGS__)

	// --- CheatClient Implementation ---

	CheatClient::CheatClient()
		: socket_(INVALID_SOCKET), bConnected(false), bRunning(false),
		bAutoReconnect(false), reconnectInterval(5000)
	{
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}

	CheatClient::~CheatClient()
	{
		Disconnect();
		WSACleanup();
	}

	bool CheatClient::Connect(const std::string& host, int port, const std::string& authToken, const std::string& hwidHash)
	{
		this->host = host;
		this->port = port;
		this->authToken = authToken;
		this->hwidHash = hwidHash;

		socket_ = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (socket_ == INVALID_SOCKET)
		{
			WS_LOG("socket() failed (error=%d)", WSAGetLastError());
			return false;
		}

		// Resolve hostname
		struct hostent* hostEntry = gethostbyname(host.c_str());
		if (!hostEntry)
		{
			WS_LOG("gethostbyname failed for %s (error=%d)", host.c_str(), WSAGetLastError());
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
			return false;
		}

		sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(port);
		memcpy(&serverAddr.sin_addr, hostEntry->h_addr, hostEntry->h_length);

		WS_LOG("Connecting to %s:%d...", host.c_str(), port);
		if (connect(socket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		{
			WS_LOG("connect() failed (error=%d)", WSAGetLastError());
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
			return false;
		}
		WS_LOG("TCP connected");

		// Perform WebSocket handshake
		if (!PerformHandshake())
		{
			WS_LOG("WebSocket handshake failed");
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
			return false;
		}
		WS_LOG("WebSocket handshake OK");

		bConnected = true;
		bRunning = true;

		// Remove receive timeout after handshake
		DWORD no_timeout = 0;
		setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&no_timeout, sizeof(no_timeout));

		// Send auth message with HWID
		std::string authMsg;
		if (!hwidHash.empty()) {
			authMsg = "{\"type\":\"auth\",\"token\":\"" + authToken + "\",\"hwid_hash\":\"" + hwidHash + "\"}";
		} else {
			authMsg = "{\"type\":\"auth\",\"token\":\"" + authToken + "\"}";
		}
		Send(authMsg);

		// Start receive thread (safe if reconnecting from an older loop)
		if (recvThread.joinable())
		{
			if (recvThread.get_id() == std::this_thread::get_id())
				recvThread.detach();
			else
				recvThread.join();
		}
		recvThread = std::thread(&CheatClient::ReceiveLoop, this);

		return true;
	}

	void CheatClient::Disconnect()
	{
		bRunning = false;
		bConnected = false;

		if (socket_ != INVALID_SOCKET)
		{
			// Send close frame
			std::vector<uint8_t> closeFrame = { 0x88, 0x00 };
			send(socket_, (const char*)closeFrame.data(), (int)closeFrame.size(), 0);
			closesocket(socket_);
			socket_ = INVALID_SOCKET;
		}

		if (recvThread.joinable())
			recvThread.join();
	}

	bool CheatClient::Send(const std::string& json)
	{
		if (!bConnected || socket_ == INVALID_SOCKET)
			return false;

		auto frame = CreateFrame(json);
		int result = send(socket_, (const char*)frame.data(), (int)frame.size(), 0);
		return result > 0;
	}

	void CheatClient::OnMessage(std::function<void(const std::string&)> callback)
	{
		messageCallback = callback;
	}

	void CheatClient::SetAutoReconnect(bool enable, int intervalMs)
	{
		bAutoReconnect = enable;
		reconnectInterval = intervalMs;
	}

	bool CheatClient::PerformHandshake()
	{
		// Set receive timeout (5 seconds)
		DWORD timeout = 5000;
		setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

		// Generate a proper random Base64-encoded WebSocket key
		const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		char wsKey[25] = {};
		for (int i = 0; i < 22; i++) wsKey[i] = b64[rand() % 64];
		wsKey[22] = '=';
		wsKey[23] = '=';

		std::string request =
			"GET /ws HTTP/1.1\r\n"
			"Host: " + host + ":" + std::to_string(port) + "\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: " + wsKey + "\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"\r\n";

		WS_LOG("Sending handshake...");
		if (send(socket_, request.c_str(), (int)request.size(), 0) == SOCKET_ERROR)
		{
			WS_LOG("send() failed (error=%d)", WSAGetLastError());
			return false;
		}

		// Read response
		char buffer[4096];
		WS_LOG("Waiting for handshake response...");
		int bytes = recv(socket_, buffer, sizeof(buffer) - 1, 0);
		if (bytes <= 0)
		{
			WS_LOG("recv() failed (error=%d, bytes=%d)", WSAGetLastError(), bytes);
			return false;
		}

		buffer[bytes] = '\0';
		std::string response(buffer);
		WS_LOG("Handshake response (%d bytes): %s", bytes, response.c_str());

		// Check for 101 Switching Protocols
		if (response.find("101") == std::string::npos)
		{
			WS_LOG("Expected 101, got:\n%s", response.c_str());
			return false;
		}

		return true;
	}

	void CheatClient::ReceiveLoop()
	{
		std::vector<uint8_t> buffer;
		uint8_t temp[8192];

		while (bRunning)
		{
			int bytes = recv(socket_, (char*)temp, sizeof(temp), 0);
			if (bytes <= 0)
			{
				if (bRunning)
				{
					bConnected = false;
					if (bAutoReconnect)
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(reconnectInterval));
						Connect(host, port, authToken, hwidHash);
					}
				}
				break;
			}

			buffer.insert(buffer.end(), temp, temp + bytes);

			// Parse WebSocket frames
			while (buffer.size() >= 2)
			{
				size_t offset = 0;
				bool fin = (buffer[offset] & 0x80) != 0;
				uint8_t opcode = buffer[offset] & 0x0F;
				offset++;

				bool masked = (buffer[offset] & 0x80) != 0;
				uint64_t payloadLength = buffer[offset] & 0x7F;
				offset++;

				if (payloadLength == 126)
				{
					if (buffer.size() < offset + 2) break;
					payloadLength = (buffer[offset] << 8) | buffer[offset + 1];
					offset += 2;
				}
				else if (payloadLength == 127)
				{
					if (buffer.size() < offset + 8) break;
					payloadLength = 0;
					for (int i = 0; i < 8; i++)
						payloadLength = (payloadLength << 8) | buffer[offset + i];
					offset += 8;
				}

				if (buffer.size() < offset + payloadLength)
					break;

				uint8_t mask[4] = { 0 };
				if (masked)
				{
					if (buffer.size() < offset + 4) break;
					memcpy(mask, &buffer[offset], 4);
					offset += 4;
				}

				std::vector<uint8_t> payload(buffer.begin() + offset, buffer.begin() + offset + payloadLength);

				if (masked)
				{
					for (uint64_t i = 0; i < payloadLength; i++)
						payload[i] ^= mask[i % 4];
				}

				// Handle opcode
				if (opcode == 0x8) // Close
				{
					bConnected = false;
					return;
				}
				else if (opcode == 0x9) // Ping
				{
					// Send masked pong frame (RFC 6455: client frames MUST be masked)
					auto pongFrame = CreateFrame("", 0xA);
					send(socket_, (const char*)pongFrame.data(), (int)pongFrame.size(), 0);
				}
				else if (opcode == 0x1) // Text
				{
					std::string text(payload.begin(), payload.end());
					if (messageCallback)
						messageCallback(text);
				}

				buffer.erase(buffer.begin(), buffer.begin() + offset + payloadLength);
			}
		}
	}

	std::vector<uint8_t> CheatClient::CreateFrame(const std::string& data, uint8_t opcode)
	{
		// Client frames MUST be masked (RFC 6455)
		std::vector<uint8_t> frame;
		uint64_t len = data.size();

		frame.push_back(0x80 | opcode);

		if (len < 126)
		{
			frame.push_back(0x80 | (uint8_t)len); // MASK bit set
		}
		else if (len < 65536)
		{
			frame.push_back(0x80 | 126); // MASK bit set
			frame.push_back((uint8_t)((len >> 8) & 0xFF));
			frame.push_back((uint8_t)(len & 0xFF));
		}
		else
		{
			frame.push_back(0x80 | 127); // MASK bit set
			for (int i = 7; i >= 0; i--)
				frame.push_back((uint8_t)((len >> (i * 8)) & 0xFF));
		}

		// Generate random 4-byte mask key
		uint8_t maskKey[4];
		for (int i = 0; i < 4; i++)
			maskKey[i] = rand() & 0xFF;

		frame.insert(frame.end(), maskKey, maskKey + 4);

		// Mask payload with mask key
		for (uint64_t i = 0; i < len; i++)
			frame.push_back(data[i] ^ maskKey[i % 4]);

		return frame;
	}

	// --- State Synchronization ---

	void SyncOptionsToJson(std::string& outJson)
	{
		std::ostringstream json;
		json << R"({"type":"state_sync","state":{)";

		// Aimbot
		json << R"("LegitBot.AimBot.Enabled":)" << (g_Options.LegitBot.AimBot.Enabled ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.StrictLock":)" << (g_Options.LegitBot.AimBot.StrictLock ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.VisibleCheck":)" << (g_Options.LegitBot.AimBot.VisibleCheck ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.TargetNPC":)" << (g_Options.LegitBot.AimBot.TargetNPC ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.ShowFov":)" << (g_Options.LegitBot.AimBot.ShowFov ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.HitBox":)" << g_Options.LegitBot.AimBot.HitBox << ",";
		json << R"("LegitBot.AimBot.FOV":)" << g_Options.LegitBot.AimBot.FOV << ",";
		json << R"("LegitBot.AimBot.MaxDistance":)" << g_Options.LegitBot.AimBot.MaxDistance << ",";
		json << R"("LegitBot.AimBot.SmoothHorizontal":)" << g_Options.LegitBot.AimBot.SmoothHorizontal << ",";
		json << R"("LegitBot.AimBot.SmoothVertical":)" << g_Options.LegitBot.AimBot.SmoothVertical << ",";
		json << R"("LegitBot.AimBot.Prediction":)" << (g_Options.LegitBot.AimBot.Prediction ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.PredictionMultiplier":)" << g_Options.LegitBot.AimBot.PredictionMultiplier << ",";
		json << R"("LegitBot.AimBot.AimAssist":)" << (g_Options.LegitBot.AimBot.AimAssist ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.AimAssistStrength":)" << g_Options.LegitBot.AimBot.AimAssistStrength << ",";
		json << R"("LegitBot.AimBot.StickyAim":)" << (g_Options.LegitBot.AimBot.StickyAim ? "true" : "false") << ",";
		json << R"("LegitBot.AimBot.StickyAimStrength":)" << g_Options.LegitBot.AimBot.StickyAimStrength << ",";

		// Silent Aim
		json << R"("LegitBot.SilentAim.Enabled":)" << (g_Options.LegitBot.SilentAim.Enabled ? "true" : "false") << ",";
		json << R"("LegitBot.SilentAim.Humanize":)" << (g_Options.LegitBot.SilentAim.Humanize ? "true" : "false") << ",";
		json << R"("LegitBot.SilentAim.VisibleCheck":)" << (g_Options.LegitBot.SilentAim.VisibleCheck ? "true" : "false") << ",";
		json << R"("LegitBot.SilentAim.ShotNPC":)" << (g_Options.LegitBot.SilentAim.ShotNPC ? "true" : "false") << ",";
		json << R"("LegitBot.SilentAim.ShowFOV":)" << (g_Options.LegitBot.SilentAim.ShowFOV ? "true" : "false") << ",";
		json << R"("LegitBot.SilentAim.HitBox":)" << g_Options.LegitBot.SilentAim.HitBox << ",";
		json << R"("LegitBot.SilentAim.Fov":)" << g_Options.LegitBot.SilentAim.Fov << ",";
		json << R"("LegitBot.SilentAim.MaxDistance":)" << g_Options.LegitBot.SilentAim.MaxDistance << ",";
		json << R"("LegitBot.SilentAim.MissChance":)" << g_Options.LegitBot.SilentAim.MissChance << ",";

		// Magic Bullets
		json << R"("LegitBot.MagicBullets.Enabled":)" << (g_Options.LegitBot.MagicBullets.Enabled ? "true" : "false") << ",";
		json << R"("LegitBot.MagicBullets.WallBang":)" << (g_Options.LegitBot.MagicBullets.WallBang ? "true" : "false") << ",";
		json << R"("LegitBot.MagicBullets.TargetNPC":)" << (g_Options.LegitBot.MagicBullets.TargetNPC ? "true" : "false") << ",";

		// Trigger
		json << R"("LegitBot.Trigger.Enabled":)" << (g_Options.LegitBot.Trigger.Enabled ? "true" : "false") << ",";
		json << R"("LegitBot.Trigger.ShotNPC":)" << (g_Options.LegitBot.Trigger.ShotNPC ? "true" : "false") << ",";
		json << R"("LegitBot.Trigger.VisibleCheck":)" << (g_Options.LegitBot.Trigger.VisibleCheck ? "true" : "false") << ",";
		json << R"("LegitBot.Trigger.MaxDistance":)" << g_Options.LegitBot.Trigger.MaxDistance << ",";
		json << R"("LegitBot.Trigger.ReactionTime":)" << g_Options.LegitBot.Trigger.ReactionTime << ",";

		// Crosshair
		json << R"("Crosshair.Enabled":)" << (g_Options.Crosshair.Enabled ? "true" : "false") << ",";
		json << R"("Crosshair.ShowDot":)" << (g_Options.Crosshair.ShowDot ? "true" : "false") << ",";
		json << R"("Crosshair.ShowLines":)" << (g_Options.Crosshair.ShowLines ? "true" : "false") << ",";
		json << R"("Crosshair.ShowOutline":)" << (g_Options.Crosshair.ShowOutline ? "true" : "false") << ",";
		json << R"("Crosshair.DynamicGap":)" << (g_Options.Crosshair.DynamicGap ? "true" : "false") << ",";
		json << R"("Crosshair.Style":)" << g_Options.Crosshair.Style << ",";
		json << R"("Crosshair.Size":)" << g_Options.Crosshair.Size << ",";
		json << R"("Crosshair.Gap":)" << g_Options.Crosshair.Gap << ",";
		json << R"("Crosshair.Thickness":)" << g_Options.Crosshair.Thickness << ",";
		json << R"("Crosshair.DotSize":)" << g_Options.Crosshair.DotSize << ",";
		json << R"("Crosshair.OutlineThickness":)" << g_Options.Crosshair.OutlineThickness << ",";
		json << R"("Crosshair.Rounding":)" << g_Options.Crosshair.Rounding << ",";

		// Visuals - Players
		json << R"("Visuals.Players.Enabled":)" << (g_Options.Visuals.Players.Enabled ? "true" : "false") << ",";
		json << R"("Visuals.Players.Toogle":)" << (g_Options.Visuals.Players.Toogle ? "true" : "false") << ",";
		json << R"("Visuals.Players.ShowNPC":)" << (g_Options.Visuals.Players.ShowNPC ? "true" : "false") << ",";
		json << R"("Visuals.Players.ExcludeDeads":)" << (g_Options.Visuals.Players.ExcludeDeads ? "true" : "false") << ",";
		json << R"("Visuals.Players.ShowLocalPlayer":)" << (g_Options.Visuals.Players.ShowLocalPlayer ? "true" : "false") << ",";
		json << R"("Visuals.Players.VisibleCheck":)" << (g_Options.Visuals.Players.VisibleCheck ? "true" : "false") << ",";
		json << R"("Visuals.Players.RenderDistance":)" << g_Options.Visuals.Players.RenderDistance << ",";
		json << R"("Visuals.Players.EnableBox":)" << (g_Options.Visuals.Players.EnableBox ? "true" : "false") << ",";
		json << R"("Visuals.Players.BoxType":)" << g_Options.Visuals.Players.BoxType << ",";
		json << R"("Visuals.Players.EnableDistance":)" << (g_Options.Visuals.Players.EnableDistance ? "true" : "false") << ",";
		json << R"("Visuals.Players.EnableHeadBol":)" << (g_Options.Visuals.Players.EnableHeadBol ? "true" : "false") << ",";
		json << R"("Visuals.Players.RadarEnabled":)" << (g_Options.Visuals.Players.RadarEnabled ? "true" : "false") << ",";
		json << R"("Visuals.Players.Skeleton":)" << (g_Options.Visuals.Players.Skeleton ? "true" : "false") << ",";
		json << R"("Visuals.Players.Name":)" << (g_Options.Visuals.Players.Name ? "true" : "false") << ",";
		json << R"("Visuals.Players.HealthBar":)" << (g_Options.Visuals.Players.HealthBar ? "true" : "false") << ",";
		json << R"("Visuals.Players.HealthBarType":)" << g_Options.Visuals.Players.HealthBarType << ",";
		json << R"("Visuals.Players.AmorBar":)" << (g_Options.Visuals.Players.AmorBar ? "true" : "false") << ",";
		json << R"("Visuals.Players.AmorBarType":)" << g_Options.Visuals.Players.AmorBarType << ",";
		json << R"("Visuals.Players.WeaponName":)" << (g_Options.Visuals.Players.WeaponName ? "true" : "false") << ",";
		json << R"("Visuals.Players.Chams":)" << (g_Options.Visuals.Players.Chams ? "true" : "false") << ",";
		json << R"("Visuals.Players.ChamsStyle":)" << g_Options.Visuals.Players.ChamsStyle << ",";
		json << R"("Visuals.Players.DamageNumbers":)" << (g_Options.Visuals.Players.DamageNumbers ? "true" : "false") << ",";

		// Visuals - Vehicle
		json << R"("Visuals.Vehicles.Enabled":)" << (g_Options.Visuals.Vehicles.Enabled ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.Toogle":)" << (g_Options.Visuals.Vehicles.Toogle ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.ShowLockState":)" << (g_Options.Visuals.Vehicles.ShowLockState ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.Name":)" << (g_Options.Visuals.Vehicles.Name ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.Distance":)" << (g_Options.Visuals.Vehicles.Distance ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.Marker":)" << (g_Options.Visuals.Vehicles.Marker ? "true" : "false") << ",";
		json << R"("Visuals.Vehicles.VisibleCheck":)" << (g_Options.Visuals.Vehicles.VisibleCheck ? "true" : "false") << ",";

		// Extras
		json << R"("Visuals.Players.LineToHead":)" << (g_Options.Visuals.Players.LineToHead ? "true" : "false") << ",";
		json << R"("Visuals.Players.CrosshairOnTarget":)" << (g_Options.Visuals.Players.CrosshairOnTarget ? "true" : "false") << ",";
		json << R"("Visuals.Players.PulseCircle":)" << (g_Options.Visuals.Players.PulseCircle ? "true" : "false") << ",";
		json << R"("Visuals.Players.RainbowSkeleton":)" << (g_Options.Visuals.Players.RainbowSkeleton ? "true" : "false") << ",";
		json << R"("Visuals.Players.SpinningRing":)" << (g_Options.Visuals.Players.SpinningRing ? "true" : "false") << ",";
		json << R"("Visuals.Players.BoxBreath":)" << (g_Options.Visuals.Players.BoxBreath ? "true" : "false") << ",";
		json << R"("Visuals.Players.HealthBar3D":)" << (g_Options.Visuals.Players.HealthBar3D ? "true" : "false") << ",";
		json << R"("Visuals.Players.EnemyArrow":)" << (g_Options.Visuals.Players.EnemyArrow ? "true" : "false") << ",";
		json << R"("Visuals.Players.DistanceRings":)" << (g_Options.Visuals.Players.DistanceRings ? "true" : "false") << ",";
		json << R"("Visuals.Players.ScanLine":)" << (g_Options.Visuals.Players.ScanLine ? "true" : "false") << ",";

		// Exploits
		json << R"("Exploits.Self.GodMode":)" << (g_Options.Exploits.Self.GodMode ? "true" : "false") << ",";
		json << R"("Exploits.Self.NoClip":)" << (g_Options.Exploits.Self.NoClip ? "true" : "false") << ",";
		json << R"("Exploits.Self.NoClipMode":)" << g_Options.Exploits.Self.NoClipMode << ",";
		json << R"("Exploits.Self.SpinBot":)" << (g_Options.Exploits.Self.SpinBot ? "true" : "false") << ",";
		json << R"("Exploits.Self.SpinBotSpeed":)" << g_Options.Exploits.Self.SpinBotSpeed << ",";
		json << R"("Exploits.Self.NoRagDoll":)" << (g_Options.Exploits.Self.NoRagDoll ? "true" : "false") << ",";
		json << R"("Exploits.Self.SeatBelt":)" << (g_Options.Exploits.Self.SeatBelt ? "true" : "false") << ",";
		json << R"("Exploits.Self.VehicleRepair":)" << (g_Options.Exploits.Self.VehicleRepair ? "true" : "false") << ",";
		json << R"("Exploits.Self.RemoveSpread":)" << (g_Options.Exploits.Self.RemoveSpread ? "true" : "false") << ",";
		json << R"("Exploits.Self.RemoveRecoil":)" << (g_Options.Exploits.Self.RemoveRecoil ? "true" : "false") << ",";
		json << R"("Exploits.Self.WeaponSize":)" << g_Options.Exploits.Self.WeaponSize << ",";

		// General
		json << R"("General.CaptureBypass":)" << (g_Options.General.CaptureBypass ? "true" : "false") << ",";
		json << R"("General.CleanTraces":)" << (g_Options.General.CleanTraces ? "true" : "false") << ",";
		json << R"("General.ShowWatermark":)" << (g_Options.General.ShowWatermark ? "true" : "false") << ",";
		json << R"("General.Vsync":)" << (g_Options.General.Vsync ? "true" : "false") << ",";
		json << R"("General.ShowKeybindList":)" << (g_Options.General.ShowKeybindList ? "true" : "false") << ",";
		json << R"("Exploits.Session.ShowSessionInfo":)" << (g_Options.Exploits.Session.ShowSessionInfo ? "true" : "false");

		json << R"(}})";

		outJson = json.str();
	}

	std::string GenerateHWID()
	{
		// Collect hardware identifiers
		std::string raw;

		// Volume serial number
		DWORD serNum = 0;
		if (GetVolumeInformationA("C:\\", NULL, 0, &serNum, NULL, NULL, NULL, 0)) {
			raw += std::to_string(serNum);
		}

		// Computer name
		char compName[MAX_COMPUTERNAME_LENGTH + 1] = {};
		DWORD compSize = sizeof(compName);
		if (GetComputerNameA(compName, &compSize)) {
			raw += compName;
		}

		// MAC address (first adapter)
		IP_ADAPTER_INFO adapterInfo[16];
		DWORD bufLen = sizeof(adapterInfo);
		if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_SUCCESS) {
			for (DWORD i = 0; i < adapterInfo[0].AddressLength; i++) {
				char buf[4];
				sprintf_s(buf, "%02X", adapterInfo[0].Address[i]);
				raw += buf;
			}
		}

		// Machine GUID from registry
		HKEY hKey;
		if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
			char guid[64] = {};
			DWORD guidSize = sizeof(guid);
			if (RegQueryValueExA(hKey, "MachineGuid", NULL, NULL, (LPBYTE)guid, &guidSize) == ERROR_SUCCESS) {
				raw += guid;
			}
			RegCloseKey(hKey);
		}

		// SHA256 hash of the raw data using Windows BCrypt
		BCRYPT_ALG_HANDLE hAlg = NULL;
		NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
		if (!BCRYPT_SUCCESS(status))
			return raw;

		DWORD hashLen = 0;
		DWORD cbData = 0;
		BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cbData, 0);

		std::vector<BYTE> hash(hashLen);
		BCryptHash(hAlg, NULL, 0, (PUCHAR)raw.c_str(), (ULONG)raw.size(), hash.data(), (ULONG)hash.size());

		char hexBuf[4];
		std::string digest;
		for (BYTE b : hash) {
			sprintf_s(hexBuf, "%02x", b);
			digest += hexBuf;
		}

		BCryptCloseAlgorithmProvider(hAlg, 0);
		return digest;
	}

	bool ApplyOptionFromJson(const std::string& json)
	{
		// Parse simple JSON key-value pairs
		// Format: {"type":"set_option","path":"LegitBot.AimBot.Enabled","value":true}
		if (json.find("\"set_option\"") == std::string::npos)
			return false;

		auto extractStr = [&](const std::string& key) -> std::string {
			size_t start = json.find(key);
			if (start == std::string::npos) return "";
			start += key.length();
			if (start >= json.length()) return "";
			if (json[start] == '"') {
				start++;
				size_t end = json.find('"', start);
				return (end == std::string::npos) ? "" : json.substr(start, end - start);
			}
			size_t end = json.find_first_of(",}", start);
			return json.substr(start, end - start);
		};

		std::string path = extractStr("\"path\":\"");
		if (path.empty()) return false;

		std::string valStr = extractStr("\"value\":");
		if (valStr.empty()) return false;

		// Apply to g_Options
		if (path == "LegitBot.AimBot.Enabled") g_Options.LegitBot.AimBot.Enabled = (valStr == "true");
		else if (path == "LegitBot.AimBot.StrictLock") g_Options.LegitBot.AimBot.StrictLock = (valStr == "true");
		else if (path == "LegitBot.AimBot.VisibleCheck") g_Options.LegitBot.AimBot.VisibleCheck = (valStr == "true");
		else if (path == "LegitBot.AimBot.TargetNPC") g_Options.LegitBot.AimBot.TargetNPC = (valStr == "true");
		else if (path == "LegitBot.AimBot.ShowFov") g_Options.LegitBot.AimBot.ShowFov = (valStr == "true");
		else if (path == "LegitBot.AimBot.HitBox") g_Options.LegitBot.AimBot.HitBox = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.FOV") g_Options.LegitBot.AimBot.FOV = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.MaxDistance") g_Options.LegitBot.AimBot.MaxDistance = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.SmoothHorizontal") g_Options.LegitBot.AimBot.SmoothHorizontal = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.SmoothVertical") g_Options.LegitBot.AimBot.SmoothVertical = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.Prediction") g_Options.LegitBot.AimBot.Prediction = (valStr == "true");
		else if (path == "LegitBot.AimBot.PredictionMultiplier") g_Options.LegitBot.AimBot.PredictionMultiplier = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.AimAssist") g_Options.LegitBot.AimBot.AimAssist = (valStr == "true");
		else if (path == "LegitBot.AimBot.AimAssistStrength") g_Options.LegitBot.AimBot.AimAssistStrength = std::stoi(valStr);
		else if (path == "LegitBot.AimBot.StickyAim") g_Options.LegitBot.AimBot.StickyAim = (valStr == "true");
		else if (path == "LegitBot.AimBot.StickyAimStrength") g_Options.LegitBot.AimBot.StickyAimStrength = std::stoi(valStr);
		else if (path == "LegitBot.SilentAim.Enabled") g_Options.LegitBot.SilentAim.Enabled = (valStr == "true");
		else if (path == "LegitBot.SilentAim.Humanize") g_Options.LegitBot.SilentAim.Humanize = (valStr == "true");
		else if (path == "LegitBot.SilentAim.VisibleCheck") g_Options.LegitBot.SilentAim.VisibleCheck = (valStr == "true");
		else if (path == "LegitBot.SilentAim.ShotNPC") g_Options.LegitBot.SilentAim.ShotNPC = (valStr == "true");
		else if (path == "LegitBot.SilentAim.ShowFOV") g_Options.LegitBot.SilentAim.ShowFOV = (valStr == "true");
		else if (path == "LegitBot.SilentAim.HitBox") g_Options.LegitBot.SilentAim.HitBox = std::stoi(valStr);
		else if (path == "LegitBot.SilentAim.Fov") g_Options.LegitBot.SilentAim.Fov = std::stoi(valStr);
		else if (path == "LegitBot.SilentAim.MaxDistance") g_Options.LegitBot.SilentAim.MaxDistance = std::stoi(valStr);
		else if (path == "LegitBot.SilentAim.MissChance") g_Options.LegitBot.SilentAim.MissChance = std::stoi(valStr);
		else if (path == "LegitBot.MagicBullets.Enabled") g_Options.LegitBot.MagicBullets.Enabled = (valStr == "true");
		else if (path == "LegitBot.MagicBullets.WallBang") g_Options.LegitBot.MagicBullets.WallBang = (valStr == "true");
		else if (path == "LegitBot.MagicBullets.TargetNPC") g_Options.LegitBot.MagicBullets.TargetNPC = (valStr == "true");
		else if (path == "LegitBot.Trigger.Enabled") g_Options.LegitBot.Trigger.Enabled = (valStr == "true");
		else if (path == "LegitBot.Trigger.ShotNPC") g_Options.LegitBot.Trigger.ShotNPC = (valStr == "true");
		else if (path == "LegitBot.Trigger.VisibleCheck") g_Options.LegitBot.Trigger.VisibleCheck = (valStr == "true");
		else if (path == "LegitBot.Trigger.MaxDistance") g_Options.LegitBot.Trigger.MaxDistance = std::stoi(valStr);
		else if (path == "LegitBot.Trigger.ReactionTime") g_Options.LegitBot.Trigger.ReactionTime = std::stoi(valStr);
		else if (path == "Crosshair.Enabled") g_Options.Crosshair.Enabled = (valStr == "true");
		else if (path == "Crosshair.ShowDot") g_Options.Crosshair.ShowDot = (valStr == "true");
		else if (path == "Crosshair.ShowLines") g_Options.Crosshair.ShowLines = (valStr == "true");
		else if (path == "Crosshair.ShowOutline") g_Options.Crosshair.ShowOutline = (valStr == "true");
		else if (path == "Crosshair.DynamicGap") g_Options.Crosshair.DynamicGap = (valStr == "true");
		else if (path == "Crosshair.Style") g_Options.Crosshair.Style = std::stoi(valStr);
		else if (path == "Crosshair.Size") g_Options.Crosshair.Size = std::stoi(valStr);
		else if (path == "Crosshair.Gap") g_Options.Crosshair.Gap = std::stoi(valStr);
		else if (path == "Crosshair.Thickness") g_Options.Crosshair.Thickness = std::stoi(valStr);
		else if (path == "Crosshair.DotSize") g_Options.Crosshair.DotSize = std::stoi(valStr);
		else if (path == "Crosshair.OutlineThickness") g_Options.Crosshair.OutlineThickness = std::stoi(valStr);
		else if (path == "Crosshair.Rounding") g_Options.Crosshair.Rounding = std::stoi(valStr);
		else if (path == "Visuals.Players.Enabled") g_Options.Visuals.Players.Enabled = (valStr == "true");
		else if (path == "Visuals.Players.Toogle") g_Options.Visuals.Players.Toogle = (valStr == "true");
		else if (path == "Visuals.Players.ShowNPC") g_Options.Visuals.Players.ShowNPC = (valStr == "true");
		else if (path == "Visuals.Players.ExcludeDeads") g_Options.Visuals.Players.ExcludeDeads = (valStr == "true");
		else if (path == "Visuals.Players.ShowLocalPlayer") g_Options.Visuals.Players.ShowLocalPlayer = (valStr == "true");
		else if (path == "Visuals.Players.VisibleCheck") g_Options.Visuals.Players.VisibleCheck = (valStr == "true");
		else if (path == "Visuals.Players.RenderDistance") g_Options.Visuals.Players.RenderDistance = std::stoi(valStr);
		else if (path == "Visuals.Players.EnableBox") g_Options.Visuals.Players.EnableBox = (valStr == "true");
		else if (path == "Visuals.Players.BoxType") g_Options.Visuals.Players.BoxType = std::stoi(valStr);
		else if (path == "Visuals.Players.EnableDistance") g_Options.Visuals.Players.EnableDistance = (valStr == "true");
		else if (path == "Visuals.Players.EnableHeadBol") g_Options.Visuals.Players.EnableHeadBol = (valStr == "true");
		else if (path == "Visuals.Players.RadarEnabled") g_Options.Visuals.Players.RadarEnabled = (valStr == "true");
		else if (path == "Visuals.Players.Skeleton") g_Options.Visuals.Players.Skeleton = (valStr == "true");
		else if (path == "Visuals.Players.Name") g_Options.Visuals.Players.Name = (valStr == "true");
		else if (path == "Visuals.Players.HealthBar") g_Options.Visuals.Players.HealthBar = (valStr == "true");
		else if (path == "Visuals.Players.HealthBarType") g_Options.Visuals.Players.HealthBarType = std::stoi(valStr);
		else if (path == "Visuals.Players.AmorBar") g_Options.Visuals.Players.AmorBar = (valStr == "true");
		else if (path == "Visuals.Players.AmorBarType") g_Options.Visuals.Players.AmorBarType = std::stoi(valStr);
		else if (path == "Visuals.Players.WeaponName") g_Options.Visuals.Players.WeaponName = (valStr == "true");
		else if (path == "Visuals.Players.Chams") g_Options.Visuals.Players.Chams = (valStr == "true");
		else if (path == "Visuals.Players.ChamsStyle") g_Options.Visuals.Players.ChamsStyle = std::stoi(valStr);
		else if (path == "Visuals.Players.DamageNumbers") g_Options.Visuals.Players.DamageNumbers = (valStr == "true");
		else if (path == "Visuals.Vehicles.Enabled") g_Options.Visuals.Vehicles.Enabled = (valStr == "true");
		else if (path == "Visuals.Vehicles.Toogle") g_Options.Visuals.Vehicles.Toogle = (valStr == "true");
		else if (path == "Visuals.Vehicles.ShowLockState") g_Options.Visuals.Vehicles.ShowLockState = (valStr == "true");
		else if (path == "Visuals.Vehicles.Name") g_Options.Visuals.Vehicles.Name = (valStr == "true");
		else if (path == "Visuals.Vehicles.Distance") g_Options.Visuals.Vehicles.Distance = (valStr == "true");
		else if (path == "Visuals.Vehicles.Marker") g_Options.Visuals.Vehicles.Marker = (valStr == "true");
		else if (path == "Visuals.Vehicles.VisibleCheck") g_Options.Visuals.Vehicles.VisibleCheck = (valStr == "true");
		else if (path == "Visuals.Players.LineToHead") g_Options.Visuals.Players.LineToHead = (valStr == "true");
		else if (path == "Visuals.Players.CrosshairOnTarget") g_Options.Visuals.Players.CrosshairOnTarget = (valStr == "true");
		else if (path == "Visuals.Players.PulseCircle") g_Options.Visuals.Players.PulseCircle = (valStr == "true");
		else if (path == "Visuals.Players.RainbowSkeleton") g_Options.Visuals.Players.RainbowSkeleton = (valStr == "true");
		else if (path == "Visuals.Players.SpinningRing") g_Options.Visuals.Players.SpinningRing = (valStr == "true");
		else if (path == "Visuals.Players.BoxBreath") g_Options.Visuals.Players.BoxBreath = (valStr == "true");
		else if (path == "Visuals.Players.HealthBar3D") g_Options.Visuals.Players.HealthBar3D = (valStr == "true");
		else if (path == "Visuals.Players.EnemyArrow") g_Options.Visuals.Players.EnemyArrow = (valStr == "true");
		else if (path == "Visuals.Players.DistanceRings") g_Options.Visuals.Players.DistanceRings = (valStr == "true");
		else if (path == "Visuals.Players.ScanLine") g_Options.Visuals.Players.ScanLine = (valStr == "true");
		else if (path == "Exploits.Self.GodMode") g_Options.Exploits.Self.GodMode = (valStr == "true");
		else if (path == "Exploits.Self.NoClip") g_Options.Exploits.Self.NoClip = (valStr == "true");
		else if (path == "Exploits.Self.NoClipMode") g_Options.Exploits.Self.NoClipMode = std::stoi(valStr);
		else if (path == "Exploits.Self.SpinBot") g_Options.Exploits.Self.SpinBot = (valStr == "true");
		else if (path == "Exploits.Self.SpinBotSpeed") g_Options.Exploits.Self.SpinBotSpeed = std::stof(valStr);
		else if (path == "Exploits.Self.NoRagDoll") g_Options.Exploits.Self.NoRagDoll = (valStr == "true");
		else if (path == "Exploits.Self.SeatBelt") g_Options.Exploits.Self.SeatBelt = (valStr == "true");
		else if (path == "Exploits.Self.VehicleRepair") g_Options.Exploits.Self.VehicleRepair = (valStr == "true");
		else if (path == "Exploits.Self.RemoveSpread") g_Options.Exploits.Self.RemoveSpread = (valStr == "true");
		else if (path == "Exploits.Self.RemoveRecoil") g_Options.Exploits.Self.RemoveRecoil = (valStr == "true");
		else if (path == "Exploits.Self.WeaponSize") g_Options.Exploits.Self.WeaponSize = std::stof(valStr);
		else if (path == "General.CaptureBypass") g_Options.General.CaptureBypass = (valStr == "true");
		else if (path == "General.CleanTraces") g_Options.General.CleanTraces = (valStr == "true");
		else if (path == "General.ShowWatermark") g_Options.General.ShowWatermark = (valStr == "true");
		else if (path == "General.Vsync") g_Options.General.Vsync = (valStr == "true");
		else if (path == "General.ShowKeybindList") g_Options.General.ShowKeybindList = (valStr == "true");
		else if (path == "Exploits.Session.ShowSessionInfo") g_Options.Exploits.Session.ShowSessionInfo = (valStr == "true");

		return true;
	}
}
