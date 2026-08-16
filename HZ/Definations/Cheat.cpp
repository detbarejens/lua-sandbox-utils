#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "Cheat.hpp"
#include "../FiveM-External.hpp"
#include "../Render/CustomWidgets/Notify.hpp"
#include "../FrameWork/Utilities/Discord.hpp"
#include "../Render/Interface.hpp"
#include "../Render/GameMirror.hpp"
#include "../Features/LegitBot/Magic.hpp"
#include "../WebControl/WebServer.hpp"
#include "../Utils/LocalConfig.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_internal.h"
#include "../ImGui/imgui_impl_dx11.h"
#include "../ImGui/imgui_impl_win32.h"
#include <cstdio>
#include <atomic>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <winhttp.h>
#include "../Utils/Memory.hpp"

#if defined(LICENSE_AUTH) && LICENSE_AUTH
#include "Auth.hpp"
#endif
#if defined(FRIENDS_BUILD) && FRIENDS_BUILD
#include "FriendsLicense.hpp"
#include "FriendsUpdate.hpp"
#endif
#include "../Utils/DebugLog.hpp"
#include "../Definations/Brand.hpp"
#include "TrinityLock.hpp"
#if defined(TRINITY_DEV) && TRINITY_DEV
#include "../Features/Dev/DevBridge.hpp"
#endif

#pragma comment(lib, "winhttp.lib")

#define DBG(fmt, ...) MELLO_DBG("[Cheat] " fmt, ##__VA_ARGS__)
#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 3000
#define DEFAULT_AUTH_SERVER "127.0.0.1"
#define WS_PORT 3001

WebControl::CheatClient* g_CheatClient = nullptr;
std::string g_AuthToken;
std::string g_ServerHost = SERVER_HOST;

#if defined(LICENSE_AUTH) && LICENSE_AUTH
Cheat::AuthState Cheat::g_AuthState;
#endif

namespace Cheat
{
	static std::string JsonEscape(const std::string& value)
	{
		std::string escaped;
		escaped.reserve(value.size() + 8);
		for (char c : value)
		{
			if (c == '\\' || c == '"') escaped.push_back('\\');
			escaped.push_back(c);
		}
		return escaped;
	}

	static std::string ExtractJsonStringField(const std::string& json, const char* field)
	{
		const std::string needle = std::string("\"") + field + "\":\"";
		const size_t start = json.find(needle);
		if (start == std::string::npos) return {};
		size_t pos = start + needle.size();
		std::string out;
		while (pos < json.size())
		{
			if (json[pos] == '\\' && pos + 1 < json.size()) { out.push_back(json[pos + 1]); pos += 2; continue; }
			if (json[pos] == '"') break;
			out.push_back(json[pos++]);
		}
		return out;
	}

#if defined(LICENSE_AUTH) && LICENSE_AUTH
	std::string GetHwidHash()
	{
		if (g_AuthState.HwidHash.empty())
			g_AuthState.HwidHash = WebControl::GenerateHWID();
		return g_AuthState.HwidHash;
	}

	void SaveLicenseKey(const std::string& licenseKey)
	{
		try
		{
			const std::string path = LocalConfig::GetConfigDirectory() + "license.key";
			std::filesystem::create_directories(std::filesystem::path(path).parent_path());
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			if (file) file.write(licenseKey.data(), static_cast<std::streamsize>(licenseKey.size()));
		}
		catch (...) {}
	}

	bool LoadSavedLicenseKey(std::string& outKey)
	{
		try
		{
			const std::string path = LocalConfig::GetConfigDirectory() + "license.key";
			std::ifstream file(path, std::ios::binary);
			if (!file) return false;
			outKey.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
			while (!outKey.empty() && (outKey.back() == '\r' || outKey.back() == '\n' || outKey.back() == ' '))
				outKey.pop_back();
			return !outKey.empty();
		}
		catch (...) { return false; }
	}
#endif
	static void RequestShutdown(const char* reason)
	{
		bool expected = false;
		if (!g_Options.General.ShutDown.compare_exchange_strong(expected, true))
			return;

		DBG("%s", reason);

		if (HWND overlay = FrameWork::Overlay::GetOverlayWindow())
			PostMessage(overlay, WM_NULL, 0, 0);
	}

	static bool IsBenignException(DWORD code)
	{
		return code == 0x406D1388  // MS_VC_EXCEPTION / SetThreadName
			|| code == 0x40010006  // DBG_PRINTEXCEPTION_C
			|| code == 0x4001000A; // DBG_PRINTEXCEPTION_WIDE_C
	}

	static LONG CALLBACK HzCrashFilter(EXCEPTION_POINTERS* info)
	{
		if (info && info->ExceptionRecord && IsBenignException(info->ExceptionRecord->ExceptionCode))
			return EXCEPTION_CONTINUE_EXECUTION;

		if (info && info->ExceptionRecord)
		{
			DBG("UNHANDLED code=0x%08X addr=%p",
				info->ExceptionRecord->ExceptionCode,
				info->ExceptionRecord->ExceptionAddress);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	static LONG CALLBACK HzVectoredHandler(EXCEPTION_POINTERS* info)
	{
		if (info && info->ExceptionRecord && IsBenignException(info->ExceptionRecord->ExceptionCode))
			return EXCEPTION_CONTINUE_EXECUTION;
		return EXCEPTION_CONTINUE_SEARCH;
	}

	static void StartGameExitWatcher()
	{
		static std::atomic<bool> started{ false };
		if (started.exchange(true))
			return;

		std::thread([]()
		{
			int stableTicks = 0;
			int inactiveTicks = 0;
			bool armed = false;
			constexpr int requiredStableTicks = 4;
			constexpr int requiredInactiveTicks = 8;

			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				const bool sessionActive = FrameWork::Memory::IsGameSessionActive();

				if (!armed)
				{
					if (sessionActive)
					{
						if (++stableTicks >= requiredStableTicks)
							armed = true;
					}
					else
					{
						stableTicks = 0;
					}
				}
				else if (!sessionActive)
				{
					if (++inactiveTicks >= requiredInactiveTicks)
					{
						RequestShutdown("Game session ended. Shutting down...");
						break;
					}
				}
				else
				{
					inactiveTicks = 0;
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		}).detach();
	}

	static void SafeIntialize()
	{
		__try { g_Fivem.Intialize(); } __except(EXCEPTION_EXECUTE_HANDLER) { DBG("Intialize crashed, retrying..."); }
	}

	static void SafeUpdateEntities()
	{
		__try { g_Fivem.UpdateEntities(); } __except(EXCEPTION_EXECUTE_HANDLER) { DBG("UpdateEntities crashed, skipping"); }
	}

	static void SafeUpdateVehicles()
	{
		__try { g_Fivem.UpdateVehicles(); } __except(EXCEPTION_EXECUTE_HANDLER) { DBG("UpdateVehicles crashed, skipping"); }
	}

	static void SafeRenderAlerts(bool menuOpen)
	{
		__try
		{
			Cheat::ObserverAlert::Render(menuOpen);
			Cheat::ObserverAlert::RenderAimingAlert(menuOpen);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DBG("Alert render crashed, skipping");
		}
	}

	static void SafeRenderInterface(FrameWork::Interface& ui)
	{
		__try
		{
			if (ui.bShowAuth)
				ui.RenderAuthScreen();
			else if (ui.bShowLoading)
				ui.RenderLoadingScreen();
			else
				ui.RenderGui();
			ui.HandleMenuKey();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DBG("Interface render crashed, skipping frame");
		}
	}

	static void RecoverImGuiFrame()
	{
		__try
		{
			ImGuiContext* ctx = ImGui::GetCurrentContext();
			if (!ctx || !ctx->WithinFrameScope)
				return;
			while (ctx->CurrentWindowStack.Size > 0)
				ImGui::End();
			if (ctx->WithinFrameScope)
				ImGui::EndFrame();
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	static int SafeNewFrame()
	{
		__try
		{
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			return 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DBG("ImGui NewFrame crashed");
			RecoverImGuiFrame();
			return -1;
		}
	}

	static int SafeFinishFrame()
	{
		__try
		{
			ImGui::EndFrame();
			ImGui::Render();
			if (!FrameWork::Overlay::IsRenderReady())
				return 1;
			FrameWork::Overlay::dxRefresh();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			if (IDXGISwapChain* swapChain = FrameWork::Overlay::dxGetSwapChain())
			{
				const HRESULT hr = swapChain->Present(0, 0U);
				if (FAILED(hr))
					DBG("Present failed: 0x%X", hr);
			}
			return 0;
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			DBG("ImGui Render/Present crashed");
			RecoverImGuiFrame();
			return -1;
		}
	}

	void HandleWSMessage(const std::string& message)
	{
		DBG("[WS] Received: %s", message.c_str());

		if (WebControl::ApplyOptionFromJson(message))
			return;

		if (message.find("\"command\"") != std::string::npos && message.find("\"unload\"") != std::string::npos)
		{
			g_Options.General.ShutDown = true;
		}
	}

#if defined(LICENSE_AUTH) && LICENSE_AUTH
	static std::string GetExeDirectory()
	{
		char path[MAX_PATH]{};
		if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
			return {};
		std::string full(path);
		const size_t pos = full.find_last_of("\\/");
		return pos == std::string::npos ? std::string{} : full.substr(0, pos);
	}

	static bool ReadAuthServerFile(const std::string& path, std::string& outHost)
	{
		std::ifstream file(path);
		std::string host;
		if (!file || !std::getline(file, host))
			return false;

		while (!host.empty() && (host.back() == '\r' || host.back() == '\n' || host.back() == ' '))
			host.pop_back();

		if (host.empty() || host[0] == '#')
			return false;

		outHost = host;
		return true;
	}

	static std::string ResolveAuthServerHost()
	{
		std::string host;

		const std::string exeDir = GetExeDirectory();
		if (!exeDir.empty() && ReadAuthServerFile(exeDir + "\\auth_server.txt", host))
			return host;

		try
		{
			if (ReadAuthServerFile(LocalConfig::GetConfigDirectory() + "auth_server.txt", host))
				return host;
		}
		catch (...) {}

		return DEFAULT_AUTH_SERVER;
	}
#endif

	bool FetchAuthToken(const std::string& host, int port, const std::string& hwidHash, const std::string& licenseKey, std::string& outError)
	{
#if defined(LICENSE_AUTH) && LICENSE_AUTH
		std::string body = licenseKey.empty()
			? ("{\"hwid_hash\":\"" + hwidHash + "\"}")
			: ("{\"key\":\"" + JsonEscape(licenseKey) + "\",\"hwid_hash\":\"" + hwidHash + "\"}");
#else
		(void)licenseKey;
		std::string body = "{\"hwid_hash\":\"" + hwidHash + "\"}";
#endif

		HINTERNET hSession = WinHttpOpen(BRAND_HTTP_UA, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		if (!hSession) { outError = "Network initialization failed"; DBG("[WinHTTP] WinHttpOpen failed (error=%lu)", GetLastError()); return false; }

		constexpr DWORD timeoutMs = 10000;
		WinHttpSetTimeouts(hSession, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

		std::wstring whost(host.begin(), host.end());
		HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), port, 0);
		if (!hConnect)
		{
			outError = "Could not reach auth server at " + host + ":" + std::to_string(port);
			DBG("[WinHTTP] WinHttpConnect failed (error=%lu)", GetLastError());
			WinHttpCloseHandle(hSession);
			return false;
		}

		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/session/auth-token", NULL, NULL, NULL, 0);
		if (!hRequest)
		{
			outError = "Failed to create auth request";
			DBG("[WinHTTP] WinHttpOpenRequest failed (error=%lu)", GetLastError());
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		LPCWSTR headers = L"Content-Type: application/json\r\n";
		if (!WinHttpSendRequest(hRequest, headers, wcslen(headers), (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0))
		{
			outError = "Could not reach auth server at " + host + ":" + std::to_string(port) + ". Is the backend online?";
			DBG("[WinHTTP] WinHttpSendRequest failed (error=%lu)", GetLastError());
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		if (!WinHttpReceiveResponse(hRequest, NULL))
		{
			outError = "No response from auth server";
			DBG("[WinHTTP] WinHttpReceiveResponse failed (error=%lu)", GetLastError());
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return false;
		}

		std::string response;
		DWORD bytesRead = 0;
		char buffer[4096];
		while (WinHttpReadData(hRequest, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0)
		{
			buffer[bytesRead] = '\0';
			response += buffer;
		}

		DWORD statusCode = 0;
		DWORD statusSize = sizeof(statusCode);
		WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &statusCode, &statusSize, NULL);
		DBG("[WinHTTP] HTTP %lu, body: %s", statusCode, response.c_str());

		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		if (statusCode != 200)
		{
			outError = ExtractJsonStringField(response, "error");
			if (outError.empty()) outError = "Authentication failed (HTTP " + std::to_string(statusCode) + ")";
			return false;
		}

		size_t tokenStart = response.find("\"auth_token\":\"");
		if (tokenStart == std::string::npos) { outError = "Invalid server response"; DBG("[WinHTTP] auth_token not found in response"); return false; }
		tokenStart += 14;
		size_t tokenEnd = response.find("\"", tokenStart);
		if (tokenEnd == std::string::npos) { outError = "Invalid server response"; return false; }

		g_AuthToken = response.substr(tokenStart, tokenEnd - tokenStart);
		DBG("Auth token obtained: %s", g_AuthToken.c_str());
		outError.clear();
		return !g_AuthToken.empty();
	}

#if defined(LICENSE_AUTH) && LICENSE_AUTH
	bool TryAuthenticateWithKey(const std::string& licenseKey, std::string& outError)
	{
		if (licenseKey.empty()) { outError = "Enter a license key."; return false; }

		const std::string hwidHash = GetHwidHash();
#if defined(FRIENDS_BUILD) && FRIENDS_BUILD
		if (!FriendsLicense::Activate(licenseKey, hwidHash, outError))
		{
			if (outError.empty()) outError = "Authentication failed.";
			return false;
		}
#else
		if (!FetchAuthToken(g_ServerHost, SERVER_PORT, hwidHash, licenseKey, outError))
		{
			if (outError.empty()) outError = "Authentication failed.";
			return false;
		}
#endif

		g_AuthState.LicenseKey = licenseKey;
		g_AuthState.Authenticated = true;
		SaveLicenseKey(licenseKey);
#if !defined(FRIENDS_BUILD) || !FRIENDS_BUILD
		std::thread(StartWebControlAfterAuth).detach();
#endif
		return true;
	}

	void BeginAuthenticateWithKeyAsync(const std::string& licenseKey)
	{
		if (g_AuthState.Authenticating.load(std::memory_order_relaxed))
			return;

		g_AuthState.Authenticating = true;
		{
			std::lock_guard<std::mutex> lock(g_AuthState.ErrorMutex);
			g_AuthState.PendingError.clear();
		}

		std::thread([licenseKey]()
		{
			std::string error;
			if (TryAuthenticateWithKey(licenseKey, error))
			{
				StartFeatureThreads();
				g_AuthState.AuthUiSuccess = true;
			}
			else
			{
				std::lock_guard<std::mutex> lock(g_AuthState.ErrorMutex);
				g_AuthState.PendingError = error.empty() ? "Authentication failed." : error;
			}
			g_AuthState.Authenticating = false;
		}).detach();
	}

	void PollAuthUiState(bool& outSuccess, std::string& outError, bool& outBusy)
	{
		outSuccess = g_AuthState.AuthUiSuccess.exchange(false, std::memory_order_relaxed);
		outBusy = g_AuthState.Authenticating.load(std::memory_order_relaxed);
		std::lock_guard<std::mutex> lock(g_AuthState.ErrorMutex);
		if (!g_AuthState.PendingError.empty())
		{
			outError = g_AuthState.PendingError;
			g_AuthState.PendingError.clear();
		}
	}
#endif

	void StartWebControlAfterAuth()
	{
		std::string hwidHash = WebControl::GenerateHWID();
		DBG("HWID: %s", hwidHash.c_str());

#if defined(LICENSE_AUTH) && LICENSE_AUTH
		if (g_AuthToken.empty())
		{
			DBG("Web control skipped: not authenticated.");
			return;
		}
#else
		DBG("Fetching auth token from %s:%d...", g_ServerHost.c_str(), SERVER_PORT);
		std::string authError;
		if (!FetchAuthToken(g_ServerHost, SERVER_PORT, hwidHash, "", authError))
		{
			DBG("Failed to obtain auth token. Web control disabled.");
			DBG("Make sure the backend is running at %s:%d", g_ServerHost.c_str(), SERVER_PORT);
			return;
		}
#endif

		DBG("Connecting to backend WebSocket server at ws://%s:%d...", g_ServerHost.c_str(), WS_PORT);
		g_CheatClient = new WebControl::CheatClient();
		g_CheatClient->OnMessage(HandleWSMessage);
		g_CheatClient->SetAutoReconnect(true, 5000);
		if (!g_CheatClient->Connect(g_ServerHost, WS_PORT, g_AuthToken, hwidHash))
		{
			DBG("Failed to connect to WebSocket server at %s:%d", g_ServerHost.c_str(), WS_PORT);
		}

		std::thread([]() {
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				if (g_CheatClient && g_CheatClient->IsConnected())
				{
					std::string stateJson;
					WebControl::SyncOptionsToJson(stateJson);
					g_CheatClient->Send(stateJson);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		}).detach();
	}

	static void SafeAimBotRun()
	{
		__try { AimBot::RunThread(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("AimBot thread crashed"); }
	}

	static void SafeSilentAimRun()
	{
		__try { SilentAim::RunThread(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("SilentAim thread crashed"); }
	}

	static void SafeTriggerBotRun()
	{
		__try { TriggerBot::RunThread(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("TriggerBot thread crashed"); }
	}

	static void SafeMagicRun()
	{
		__try { MagicBullets::Start(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("Magic thread crashed"); }
	}

	static void SafePlayerNamesRun()
	{
		__try { g_Fivem.UpdatePlayerNamesThread(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("PlayerNames thread crashed"); }
	}

	static void SafeCarLockOnce()
	{
		__try { Cheat::Exploits::ToggleCarLock(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("CarLock crashed"); }
	}

	static void SafeExploitsOnce()
	{
		__try { Exploits::RunThread(); }
		__except (EXCEPTION_EXECUTE_HANDLER) { DBG("Exploits thread crashed"); }
	}

	void StartFeatureThreads()
	{
		static std::atomic<bool> started{ false };
		if (started.exchange(true))
			return;

		StartGameExitWatcher();
		DBG("Starting feature threads");

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeAimBotRun();
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeSilentAimRun();
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeTriggerBotRun();
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeMagicRun();
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafePlayerNamesRun();
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeCarLockOnce();
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			}
		}).detach();

		std::thread([]()
		{
			while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				SafeExploitsOnce();
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
		}).detach();
	}


	void InstallExceptionHandlers()
	{
		static bool installed = false;
		if (installed)
			return;
		installed = true;
		AddVectoredExceptionHandler(1, HzVectoredHandler);
		SetUnhandledExceptionFilter(HzCrashFilter);
	}

	void Initialize()
	{
		InstallExceptionHandlers();
		DBG("Initialize begin");

#if defined(FRIENDS_BUILD) && FRIENDS_BUILD
		FriendsUpdate::TryApplyAtStartup();
#endif
#if defined(PRODUCT_VARIANT_C) && PRODUCT_VARIANT_C
		{
			std::string lockError;
			if (!TrinityLock::VerifyOrBind(lockError))
			{
				MessageBoxA(nullptr, lockError.c_str(), BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONERROR);
				ExitProcess(0);
			}
		}
#endif
#if defined(LICENSE_AUTH) && LICENSE_AUTH && (!defined(FRIENDS_BUILD) || !FRIENDS_BUILD)
		g_ServerHost = ResolveAuthServerHost();
#endif
		DBG("Waiting for FiveM...");

#if defined(TRINITY_DEV) && TRINITY_DEV
		DevBridge::Start();
#endif


#if defined(PRODUCT_VARIANT_C) && PRODUCT_VARIANT_C && defined(TRINITY_HWID_LOCK) && TRINITY_HWID_LOCK && !defined(TRINITY_DEV)
		if (!FrameWork::Memory::IsGameSessionActive())
		{
			MessageBoxA(nullptr, "Open your game first.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONWARNING);
			ExitProcess(0);
		}
#endif

		const auto waitStart = std::chrono::steady_clock::now();
		while (!g_Fivem.IsInitialized())
		{
			SafeIntialize();
			FrameWork::Discord::GetDcName();
			if (!g_Fivem.IsInitialized())
			{
				if (std::chrono::steady_clock::now() - waitStart > std::chrono::seconds(12))
				{
					MessageBoxA(nullptr, "Could not attach to FiveM.\nJoin a server, then click Load again.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONWARNING);
					ExitProcess(0);
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(400));
			}
		}

#if defined(TRINITY_HWID_LOCK) && TRINITY_HWID_LOCK && !defined(TRINITY_DEV)
		// Let the GTA process finish loading before overlay attach.
		std::this_thread::sleep_for(std::chrono::seconds(2));
#endif

		DBG("FiveM found! PID=%d ModuleBase=0x%llX Version=%d",
			g_Fivem.GetPid(), g_Fivem.GetModuleBase(), g_Fivem.GetGameVersion());

		DBG("Setting up overlay...");
		for (int attempt = 0; attempt < 60 && !FrameWork::Overlay::IsSettuped(); ++attempt)
		{
			if (attempt > 0)
				DBG("Retrying overlay setup (%d/60)...", attempt + 1);

			FrameWork::Overlay::Setup(g_Fivem.GetPid());
			if (!FrameWork::Overlay::IsSettuped())
				std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		DBG("Overlay::Setup done, IsSettuped=%d", FrameWork::Overlay::IsSettuped());

		g_Options.General.CaptureBypass = true;
		FrameWork::Overlay::EnsureNvidiaCapturePath();
		FrameWork::Overlay::Initialize();
		DBG("Overlay::Initialize done, IsInitialized=%d", FrameWork::Overlay::IsInitialized());

		if (!FrameWork::Overlay::IsInitialized())
		{
			MessageBoxA(nullptr, "Could not find the FiveM game window.\nMake sure you are fully in-game, then try again.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}

		if (!FrameWork::Overlay::IsRenderReady())
		{
			MessageBoxA(nullptr, "DirectX initialization failed.\nUpdate your graphics drivers and try again.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONERROR);
			ExitProcess(0);
		}

		if (HWND consoleWindow = GetConsoleWindow())
			ShowWindow(consoleWindow, SW_HIDE);

		g_Options.General.Vsync = false;

		if (FrameWork::Overlay::IsRenderReady())
		{
			FrameWork::GameMirror::Initialize(
				FrameWork::Overlay::dxGetDevice(),
				FrameWork::Overlay::dxGetDeviceContext());

			DBG("Creating Interface...");
			FrameWork::Interface Interface(FrameWork::Overlay::GetOverlayWindow(), FrameWork::Overlay::GetTargetWindow(), FrameWork::Overlay::dxGetDevice(), FrameWork::Overlay::dxGetDeviceContext());
			DBG("Interface created, calling UpdateStyle...");
			Interface.UpdateStyle();
			g_Options.General.CaptureBypass = true;
			FrameWork::Overlay::ApplyCaptureBypass();
			DBG("UpdateStyle done, setting up WndProc hook...");
			FrameWork::Overlay::SetupWindowProcHook(std::bind(&FrameWork::Interface::WindowProc, &Interface, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

#if defined(LICENSE_AUTH) && LICENSE_AUTH
			if (g_AuthState.Authenticated)
			{
				Interface.bShowAuth = false;
			}
			else
			{
				Interface.bShowAuth = true;
				GetHwidHash();
				strncpy_s(Interface.HwidDisplayBuffer, g_AuthState.HwidHash.c_str(), _TRUNCATE);
				std::string savedKey;
				if (LoadSavedLicenseKey(savedKey))
					strncpy_s(Interface.LicenseKeyBuffer, savedKey.c_str(), _TRUNCATE);
			}
#endif
			Interface.SetMenuOpen(false);
			Interface.RefreshWindowStyle();
			Interface.DelayedMenuOpenFrames = 8;
			StartGameExitWatcher();
			DBG("Entering render loop...");

			auto TargetTP = std::chrono::steady_clock::now();
			int frameCount = 0;

			MSG Message;
			ZeroMemory(&Message, sizeof(Message));
			while (Message.message != WM_QUIT && !g_Options.General.ShutDown.load(std::memory_order_relaxed))
			{
				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;

				TargetTP += std::chrono::microseconds(std::chrono::seconds(1)) / int(144.f);
				std::this_thread::sleep_until(TargetTP);

				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;

				if (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&Message);
					DispatchMessage(&Message);
				}

				if (g_Options.General.ShutDown.load(std::memory_order_relaxed))
					break;

				if (!FrameWork::Overlay::IsRenderReady())
					continue;

				if (frameCount >= 90)
				{
					if (frameCount == 90) DBG("Starting entity updates");
					SafeUpdateEntities();
					SafeUpdateVehicles();
				}

				if (frameCount >= 10 && Interface.ResizeHeight != 0 && Interface.ResizeWidht != 0
					&& Interface.ResizeWidht < 8192 && Interface.ResizeHeight < 8192)
				{
					FrameWork::Overlay::dxCleanupRenderTarget();
					if (auto* swap = FrameWork::Overlay::dxGetSwapChain())
						swap->ResizeBuffers(0, Interface.ResizeWidht, Interface.ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
					Interface.ResizeHeight = Interface.ResizeWidht = 0;
					FrameWork::Overlay::dxCreateRenderTarget();
					if (!FrameWork::Overlay::IsRenderReady())
						continue;
				}

				if (frameCount >= 45)
					FrameWork::Overlay::UpdateWindowPos();

				if ((frameCount & 7) == 0)
					FrameWork::Overlay::ApplyCaptureBypass();

				static bool s_wasSecondMonitorEsp = false;
				const bool secondMonitorEspActive = g_Options.General.SecondMonitor && g_Options.General.MonitorIndex >= 0;
				if (secondMonitorEspActive != s_wasSecondMonitorEsp)
				{
					Interface.RefreshWindowStyle();
					s_wasSecondMonitorEsp = secondMonitorEspActive;
				}

			// Toggle ESP keys
				if (g_Options.Visuals.Players.Toogle && GetAsyncKeyState(g_Options.Visuals.Players.ToggleKey) & 1)
					g_Options.Visuals.Players.Enabled = !g_Options.Visuals.Players.Enabled;
				if (g_Options.Visuals.Vehicles.Toogle && GetAsyncKeyState(g_Options.Visuals.Vehicles.ToggleKey) & 1)
					g_Options.Visuals.Vehicles.Enabled = !g_Options.Visuals.Vehicles.Enabled;

				if (frameCount == 0) DBG("Frame 1 - before ImGui NewFrame");
				if (SafeNewFrame() != 0)
				{
					frameCount++;
					continue;
				}
				if (frameCount == 0) DBG("Frame 1 - ImGui NewFrame OK");

				// Draw FOV circles / ESP only after login
				if (!Interface.bShowAuth)
				{
					const bool secondMonitorEsp = g_Options.General.SecondMonitor && g_Options.General.MonitorIndex >= 0;

					if (secondMonitorEsp)
					{
						FrameWork::GameMirror::Update(
							FrameWork::Overlay::GetTargetWindow(),
							FrameWork::Overlay::GetOverlayWindow());
						if (FrameWork::GameMirror::IsReady())
						{
							ImGui::GetBackgroundDrawList()->AddImage(
								reinterpret_cast<ImTextureID>(FrameWork::GameMirror::GetSrv()),
								ImVec2{ 0.f, 0.f },
								ImGui::GetIO().DisplaySize,
								ImVec2{ 0.f, 0.f },
								ImVec2{ 1.f, 1.f },
								IM_COL32_WHITE
							);
						}
					}

					{
						ImDrawList* overlayDraw = ImGui::GetBackgroundDrawList();
						const ImVec2 fovCenter(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
						if (g_Options.LegitBot.AimBot.ShowFov)
							overlayDraw->AddCircle(fovCenter, (float)g_Options.LegitBot.AimBot.FOV, FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.AimBot.FovColor));
						if (g_Options.LegitBot.SilentAim.ShowFOV)
							overlayDraw->AddCircle(fovCenter, (float)g_Options.LegitBot.SilentAim.Fov, FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.SilentAim.FovColor));
						if (g_Options.LegitBot.Trigger.ShowFOV)
							overlayDraw->AddCircle(fovCenter, (float)g_Options.LegitBot.Trigger.FOV, FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.Trigger.FovColor));
						if (g_Options.LegitBot.MagicBullets.ShowFOV)
							overlayDraw->AddCircle(fovCenter, (float)g_Options.LegitBot.MagicBullets.FOV, FrameWork::Misc::Float4ToImColor(g_Options.LegitBot.MagicBullets.FovColor));
					}

					if (g_Options.Visuals.Vehicles.Enabled)
						ESP::Vehicles();
					if (g_Options.Visuals.Players.Enabled)
						ESP::Players();

					if (frameCount > 180)
						SafeRenderAlerts(Interface.GetMenuOpen());
				}

				SafeRenderInterface(Interface);

				// Crosshair
				if (!Interface.bShowAuth && g_Options.Crosshair.Enabled && !(g_Options.General.SecondMonitor && g_Options.General.MonitorIndex >= 0))
				{
					ImDrawList* cdl = ImGui::GetBackgroundDrawList();
					ImVec2 sc(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
					ImColor cc  = FrameWork::Misc::Float4ToImColor(g_Options.Crosshair.Color);
					ImColor oc  = FrameWork::Misc::Float4ToImColor(g_Options.Crosshair.OutlineColor);
					ImColor dc  = FrameWork::Misc::Float4ToImColor(g_Options.Crosshair.DotColor);
					float sz    = (float)g_Options.Crosshair.Size;
					float gap   = (float)g_Options.Crosshair.Gap;
					float thick = (float)g_Options.Crosshair.Thickness;
					float ot    = thick + g_Options.Crosshair.OutlineThickness * 2.f;
					float rounding = (float)g_Options.Crosshair.Rounding;

					if (g_Options.Crosshair.DynamicGap && g_Options.LegitBot.AimBot.Enabled && SafeCall(GetAsyncKeyState)(g_Options.LegitBot.AimBot.KeyBind))
						gap += 4.f;

					if (g_Options.Crosshair.ShowLines && (g_Options.Crosshair.Style == 0 || g_Options.Crosshair.Style == 2))
					{
						if (rounding > 0)
						{
							if (g_Options.Crosshair.ShowOutline)
							{
								cdl->AddRectFilled(ImVec2(sc.x - sz - gap, sc.y - ot/2), ImVec2(sc.x - gap, sc.y + ot/2), oc, rounding);
								cdl->AddRectFilled(ImVec2(sc.x + gap, sc.y - ot/2), ImVec2(sc.x + sz + gap, sc.y + ot/2), oc, rounding);
								cdl->AddRectFilled(ImVec2(sc.x - ot/2, sc.y - sz - gap), ImVec2(sc.x + ot/2, sc.y - gap), oc, rounding);
								cdl->AddRectFilled(ImVec2(sc.x - ot/2, sc.y + gap), ImVec2(sc.x + ot/2, sc.y + sz + gap), oc, rounding);
							}
							cdl->AddRectFilled(ImVec2(sc.x - sz - gap, sc.y - thick/2), ImVec2(sc.x - gap, sc.y + thick/2), cc, rounding);
							cdl->AddRectFilled(ImVec2(sc.x + gap, sc.y - thick/2), ImVec2(sc.x + sz + gap, sc.y + thick/2), cc, rounding);
							cdl->AddRectFilled(ImVec2(sc.x - thick/2, sc.y - sz - gap), ImVec2(sc.x + thick/2, sc.y - gap), cc, rounding);
							cdl->AddRectFilled(ImVec2(sc.x - thick/2, sc.y + gap), ImVec2(sc.x + thick/2, sc.y + sz + gap), cc, rounding);
						}
						else
						{
							if (g_Options.Crosshair.ShowOutline)
							{
								cdl->AddLine(ImVec2(sc.x - sz - gap, sc.y), ImVec2(sc.x - gap, sc.y), oc, ot);
								cdl->AddLine(ImVec2(sc.x + gap, sc.y), ImVec2(sc.x + sz + gap, sc.y), oc, ot);
								cdl->AddLine(ImVec2(sc.x, sc.y - sz - gap), ImVec2(sc.x, sc.y - gap), oc, ot);
								cdl->AddLine(ImVec2(sc.x, sc.y + gap), ImVec2(sc.x, sc.y + sz + gap), oc, ot);
							}
							cdl->AddLine(ImVec2(sc.x - sz - gap, sc.y), ImVec2(sc.x - gap, sc.y), cc, thick);
							cdl->AddLine(ImVec2(sc.x + gap, sc.y), ImVec2(sc.x + sz + gap, sc.y), cc, thick);
							cdl->AddLine(ImVec2(sc.x, sc.y - sz - gap), ImVec2(sc.x, sc.y - gap), cc, thick);
							cdl->AddLine(ImVec2(sc.x, sc.y + gap), ImVec2(sc.x, sc.y + sz + gap), cc, thick);
						}
					}

					if (g_Options.Crosshair.Style == 1 || g_Options.Crosshair.Style == 2)
					{
						if (g_Options.Crosshair.ShowOutline)
							cdl->AddCircle(sc, sz + gap, oc, 32, ot);
						cdl->AddCircle(sc, sz + gap, cc, 32, thick);
					}

					if (g_Options.Crosshair.ShowDot && g_Options.Crosshair.Style != 1)
					{
						if (rounding > 0)
						{
							if (g_Options.Crosshair.ShowOutline)
								cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize + g_Options.Crosshair.OutlineThickness, oc, 16);
							cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize, dc, 16);
						}
						else
						{
							if (g_Options.Crosshair.ShowOutline)
								cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize + g_Options.Crosshair.OutlineThickness, oc, 8);
							cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize, dc, 8);
						}
					}

					if (g_Options.Crosshair.Style == 3)
					{
						if (g_Options.Crosshair.ShowOutline)
							cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize + g_Options.Crosshair.OutlineThickness + 2.f, oc, 12);
						cdl->AddCircleFilled(sc, (float)g_Options.Crosshair.DotSize + 2.f, dc, 12);
					}
				}

				NotifyManager::Render();

				if (frameCount < 5) DBG("Frame %d - before Present", frameCount + 1);
				if (SafeFinishFrame() == 0 && frameCount == 0)
					DBG("Frame 1 - COMPLETE, loop running!");
				frameCount++;
				if ((frameCount % 120) == 0)
					DBG("Render heartbeat frame=%d menu=%d", frameCount, Interface.GetMenuOpen() ? 1 : 0);
				if (Interface.DelayedMenuOpenFrames > 0 && --Interface.DelayedMenuOpenFrames == 0)
				{
					DBG("Opening menu after overlay settle");
					Interface.SetMenuOpen(true);
				}
				if (frameCount == 60)
				{
					g_Options.General.CaptureBypass = true;
					FrameWork::Overlay::ApplyCaptureBypass();
					DBG("Capture exclusion enabled");
				}
				if (frameCount == 180)
				{
					DBG("Overlay stable, starting workers");
					StartFeatureThreads();
#if !defined(LICENSE_AUTH) || !LICENSE_AUTH
					std::thread(StartWebControlAfterAuth).detach();
#endif
				}
			}
		}
	}

	void ShutDown()
	{
		static std::atomic<bool> cleanupDone{ false };
		if (cleanupDone.exchange(true))
			return;

		g_Options.General.ShutDown = true;

		SilentAim::RestorePatch();
		MagicBullets::RestorePatch();

		if (g_Options.General.CleanTraces)
			LocalConfig::CleanDataDirectory();

		if (g_CheatClient)
		{
			g_CheatClient->Disconnect();
			delete g_CheatClient;
			g_CheatClient = nullptr;
		}

		if (FrameWork::Overlay::IsInitialized())
		{
			FrameWork::GameMirror::Shutdown();
			FrameWork::Overlay::ShutDown();
			FrameWork::Overlay::dxShutDown();
		}
	}
}
