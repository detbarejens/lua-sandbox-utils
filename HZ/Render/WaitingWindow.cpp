#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "WaitingWindow.hpp"
#include "../Definations/Brand.hpp"
#include "../Definations/TrinityLock.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_dx11.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../Utils/Memory.hpp"

#if defined(LICENSE_AUTH) && LICENSE_AUTH
#include "../Definations/Auth.hpp"
#include "../Definations/FriendsLicense.hpp"
#endif

#include "../Utils/DebugLog.hpp"

#include <d3d11.h>
#include <dwmapi.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <string>

#define HUB_DBG(fmt, ...) MELLO_DBG("[Hub] " fmt, ##__VA_ARGS__)

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace
{
	constexpr int kClientW = 720;
	constexpr int kClientH = 480;

	enum class Page
	{
		Login,
		Home
	};

	HWND g_hWnd = nullptr;
	WNDCLASSEXW g_wc{};
	wchar_t g_className[64]{};

	ID3D11Device* g_device = nullptr;
	ID3D11DeviceContext* g_context = nullptr;
	IDXGISwapChain* g_swapChain = nullptr;
	ID3D11RenderTargetView* g_rtv = nullptr;

	bool g_requestClose = false;
	bool g_readyToLoad = false;
	bool g_imguiReady = false;
	Page g_page = Page::Login;

	char g_keyBuf[256]{};
	char g_errorBuf[256]{};
	std::string g_hwid;
	std::string g_userLabel = "Guest";

	void RecreateRtv();

	LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
			return true;

		switch (msg)
		{
		case WM_ERASEBKGND:
			return 1;
		case WM_SIZE:
			if (g_swapChain && wParam != SIZE_MINIMIZED)
			{
				const UINT width = LOWORD(lParam);
				const UINT height = HIWORD(lParam);
				if (width && height)
				{
					if (g_imguiReady)
						ImGui_ImplDX11_InvalidateDeviceObjects();
					if (g_rtv)
					{
						g_rtv->Release();
						g_rtv = nullptr;
					}
					g_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
					RecreateRtv();
					if (g_imguiReady)
						ImGui_ImplDX11_CreateDeviceObjects();
				}
			}
			return 0;
		case WM_CLOSE:
			g_requestClose = true;
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		default:
			break;
		}

		return DefWindowProcW(hWnd, msg, wParam, lParam);
	}

	void RecreateRtv()
	{
		if (g_rtv)
		{
			g_rtv->Release();
			g_rtv = nullptr;
		}
		if (!g_swapChain || !g_device)
			return;
		ID3D11Texture2D* backBuffer = nullptr;
		if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))))
			return;
		g_device->CreateRenderTargetView(backBuffer, nullptr, &g_rtv);
		backBuffer->Release();
	}

	bool CreateDevice()
	{
		DXGI_SWAP_CHAIN_DESC desc{};
		desc.BufferCount = 2;
		desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.BufferDesc.RefreshRate.Numerator = 60;
		desc.BufferDesc.RefreshRate.Denominator = 1;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.OutputWindow = g_hWnd;
		desc.SampleDesc.Count = 1;
		desc.Windowed = TRUE;
		desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
		D3D_FEATURE_LEVEL featureLevel{};
		if (FAILED(D3D11CreateDeviceAndSwapChain(
			nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
			levels, 2, D3D11_SDK_VERSION,
			&desc, &g_swapChain, &g_device, &featureLevel, &g_context)))
			return false;

		RecreateRtv();
		return g_rtv != nullptr;
	}

	void CleanupDevice()
	{
		if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; }
		if (g_swapChain) { g_swapChain->Release(); g_swapChain = nullptr; }
		if (g_context) { g_context->Release(); g_context = nullptr; }
		if (g_device) { g_device->Release(); g_device = nullptr; }
	}

	void AccentButtonColors()
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.95f, 0.22f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.39f, 0.40f, 0.95f, 0.36f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.39f, 0.40f, 0.95f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.93f, 1.f, 1.f));
	}

	void OutlineButtonColors()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.4f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.f, 0.f, 0.f, 0.f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.39f, 0.40f, 0.95f, 0.12f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.39f, 0.40f, 0.95f, 0.20f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.40f, 0.95f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.39f, 0.40f, 0.95f, 0.70f));
	}

	void DrawShell(ImDrawList* dl, const ImVec2& pos, const ImVec2& sz, const char* title, const char* crumb)
	{
		const ImU32 bg = IM_COL32(18, 18, 18, 255);
		const ImU32 header = IM_COL32(20, 20, 20, 255);
		const ImU32 accent = IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255);

		dl->AddRectFilled(pos, pos + sz, bg);
		dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + 72.f), header);
		dl->AddLine(pos + ImVec2{ 0.f, 72.f }, pos + ImVec2{ sz.x, 72.f }, IM_COL32(255, 255, 255, 14));

		dl->AddText(pos + ImVec2{ 28.f, 16.f }, IM_COL32(245, 245, 247, 255), title);
		dl->AddText(pos + ImVec2{ 28.f, 38.f }, IM_COL32(140, 146, 158, 255), crumb);

		const ImVec2 avatarC = pos + ImVec2{ sz.x - 118.f, 36.f };
		dl->AddCircleFilled(avatarC, 14.f, IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 48));
		dl->AddCircle(avatarC, 14.f, accent, 0, 1.4f);
		dl->AddText(avatarC - ImVec2{ 5.f, 8.f }, accent, "H");
		dl->AddText(pos + ImVec2{ sz.x - 96.f, 20.f }, IM_COL32(245, 245, 247, 255), g_userLabel.c_str());
		dl->AddText(pos + ImVec2{ sz.x - 96.f, 38.f }, IM_COL32(140, 146, 158, 255), BRAND_USER_ROLE);
		dl->AddCircleFilled(pos + ImVec2{ sz.x - 28.f, 46.f }, 3.4f, accent);
	}

	void PresentFrame()
	{
		ImGui::Render();
		if (!g_context || !g_rtv || !g_swapChain)
			return;

		const float clear[4] = { 0.07f, 0.07f, 0.07f, 1.f };
		g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
		g_context->ClearRenderTargetView(g_rtv, clear);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		g_swapChain->Present(1, 0);
	}

	void BeginHost()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(kClientW), static_cast<float>(kClientH)));
		ImGui::Begin("##hz_hub", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
	}

	bool TrySignIn()
	{
		g_errorBuf[0] = '\0';

#if defined(LICENSE_AUTH) && LICENSE_AUTH
		if (!g_keyBuf[0])
		{
			strncpy_s(g_errorBuf, "Enter a license key.", _TRUNCATE);
			return false;
		}

		std::string error;
		if (!FriendsLicense::Activate(g_keyBuf, g_hwid, error))
		{
			strncpy_s(g_errorBuf, error.empty() ? "Invalid key." : error.c_str(), _TRUNCATE);
			return false;
		}

		Cheat::g_AuthState.LicenseKey = g_keyBuf;
		Cheat::g_AuthState.Authenticated = true;
		Cheat::SaveLicenseKey(g_keyBuf);
		g_userLabel = "Customer";
		return true;
#else
		g_userLabel = "Developer";
		return true;
#endif
	}

	void RenderLogin(const ImVec2& pos, const ImVec2& sz)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 cardMin = pos + ImVec2{ 28.f, 96.f };
		const ImVec2 cardMax = pos + ImVec2{ sz.x - 28.f, sz.y - 88.f };
		dl->AddRectFilled(cardMin, cardMax, IM_COL32(26, 26, 26, 255), 12.f);
		dl->AddRect(cardMin, cardMax, IM_COL32(255, 255, 255, 14), 12.f);

		ImGui::SetCursorPos(ImVec2(48.f, 114.f));
		ImGui::TextColored(ImVec4(0.61f, 0.64f, 0.69f, 1.f), "LICENSE KEY");
		ImGui::SetCursorPos(ImVec2(48.f, 138.f));
		ImGui::SetNextItemWidth(sz.x - 96.f);
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.11f, 1.f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.39f, 0.40f, 0.95f, 0.28f));
		ImGui::InputText("##hzkey", g_keyBuf, sizeof(g_keyBuf));
		ImGui::PopStyleColor(2);

		ImGui::SetCursorPos(ImVec2(48.f, 180.f));
		ImGui::TextColored(ImVec4(0.42f, 0.45f, 0.50f, 1.f), "Device  %s", g_hwid.empty() ? "..." : g_hwid.c_str());

		if (g_errorBuf[0])
		{
			ImGui::SetCursorPos(ImVec2(48.f, 208.f));
			ImGui::PushTextWrapPos(sz.x - 48.f);
			ImGui::TextColored(ImVec4(1.f, 0.38f, 0.38f, 1.f), "%s", g_errorBuf);
			ImGui::PopTextWrapPos();
		}

		ImGui::SetCursorPos(ImVec2(48.f, sz.y - 148.f));
		const float btnW = (sz.x - 120.f) * 0.5f;
		AccentButtonColors();
		if (ImGui::Button("Sign in", ImVec2(btnW, 40.f)))
		{
			if (TrySignIn())
				g_page = Page::Home;
		}
		ImGui::PopStyleColor(4);
		ImGui::SameLine(0.f, 12.f);
		OutlineButtonColors();
		if (ImGui::Button("Exit", ImVec2(btnW, 40.f)))
			g_requestClose = true;
		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar();
	}

	void RenderHome(const ImVec2& pos, const ImVec2& sz)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 cardMin = pos + ImVec2{ 28.f, 96.f };
		const ImVec2 cardMax = pos + ImVec2{ sz.x - 28.f, 268.f };
		dl->AddRectFilled(cardMin, cardMax, IM_COL32(26, 26, 26, 255), 12.f);
		dl->AddRect(cardMin, cardMax, IM_COL32(255, 255, 255, 14), 12.f);
		dl->AddText(cardMin + ImVec2{ 20.f, 18.f }, IM_COL32(156, 163, 175, 255), "READY");
		dl->AddText(cardMin + ImVec2{ 20.f, 44.f }, IM_COL32(245, 245, 247, 255), "Signed in. Open the game, then click Load.");
		dl->AddText(cardMin + ImVec2{ 20.f, 72.f }, IM_COL32(140, 146, 158, 255), "You need to be in-game before the overlay can attach.");
		const std::string deviceLine = g_hwid.empty() ? std::string("Device ...") : ("Device  " + g_hwid);
		dl->AddText(cardMin + ImVec2{ 20.f, 108.f }, IM_COL32(107, 114, 128, 255), deviceLine.c_str());
		dl->AddCircleFilled(cardMin + ImVec2{ cardMax.x - cardMin.x - 28.f, 32.f }, 4.f, IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255));

		if (g_errorBuf[0])
		{
			ImGui::SetCursorPos(ImVec2(48.f, 236.f));
			ImGui::PushTextWrapPos(sz.x - 48.f);
			ImGui::TextColored(ImVec4(1.f, 0.38f, 0.38f, 1.f), "%s", g_errorBuf);
			ImGui::PopTextWrapPos();
		}

		ImGui::SetCursorPos(ImVec2(28.f, sz.y - 72.f));
		const float btnW = (sz.x - 68.f) * 0.5f;
		AccentButtonColors();
		if (ImGui::Button("Load", ImVec2(btnW, 40.f)))
		{
			g_errorBuf[0] = '\0';
			const bool gameOpen = FrameWork::Memory::IsGameSessionActive()
				|| FrameWork::Memory::IsAnyFiveMRunning()
				|| FrameWork::Memory::FindGameWindow()
				|| FindWindowA("grcWindow", nullptr)
				|| FindWindowA("grcWindowEx", nullptr);

			if (!gameOpen)
			{
				strncpy_s(g_errorBuf, "Open your game first.", _TRUNCATE);
				MessageBoxA(g_hWnd, "Open your game first.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONWARNING);
			}
			else
			{
				g_readyToLoad = true;
			}
		}
		ImGui::PopStyleColor(4);
		ImGui::SameLine(0.f, 12.f);
		OutlineButtonColors();
		if (ImGui::Button("Exit", ImVec2(btnW, 40.f)))
			g_requestClose = true;
		ImGui::PopStyleColor(5);
		ImGui::PopStyleVar();
	}

	void RenderFrame()
	{
		BeginHost();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 pos = ImGui::GetWindowPos();
		const ImVec2 sz = ImGui::GetWindowSize();

		const char* title = "Sign in";
		const char* crumb = "HZ  >  License";
		if (g_page == Page::Home)
		{
			title = "Hub";
			crumb = "HZ  >  Ready";
		}
		DrawShell(dl, pos, sz, title, crumb);

		if (g_page == Page::Login)
			RenderLogin(pos, sz);
		else
			RenderHome(pos, sz);

		ImGui::End();
		PresentFrame();
	}

	bool CreateHubWindow()
	{
		swprintf_s(g_className, L"HZHubFrame_%u", GetCurrentProcessId() & 0xFFFF);

		g_wc = {};
		g_wc.cbSize = sizeof(g_wc);
		g_wc.style = CS_CLASSDC;
		g_wc.lpfnWndProc = WndProc;
		g_wc.hInstance = GetModuleHandleW(nullptr);
		g_wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		g_wc.hbrBackground = CreateSolidBrush(RGB(18, 18, 18));
		g_wc.lpszClassName = g_className;

		if (!RegisterClassExW(&g_wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			return false;

		RECT rc{ 0, 0, kClientW, kClientH };
		AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_APPWINDOW);
		const int frameW = rc.right - rc.left;
		const int frameH = rc.bottom - rc.top;
		const int x = (GetSystemMetrics(SM_CXSCREEN) - frameW) / 2;
		const int y = (GetSystemMetrics(SM_CYSCREEN) - frameH) / 2;

		g_hWnd = CreateWindowExW(
			WS_EX_APPWINDOW,
			g_className,
			L"HZ",
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE,
			x, y, frameW, frameH,
			nullptr, nullptr, g_wc.hInstance, nullptr);

		if (!g_hWnd)
			return false;

		BOOL dark = TRUE;
		DwmSetWindowAttribute(g_hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
		ShowWindow(g_hWnd, SW_SHOW);
		UpdateWindow(g_hWnd);
		SetForegroundWindow(g_hWnd);
		{
			using SetAffinityFn = BOOL(WINAPI*)(HWND, DWORD);
			HMODULE user32 = GetModuleHandleW(L"user32.dll");
			if (!user32)
				user32 = LoadLibraryW(L"user32.dll");
			if (user32)
			{
				if (auto setAffinity = reinterpret_cast<SetAffinityFn>(GetProcAddress(user32, "SetWindowDisplayAffinity")))
				{
					setAffinity(g_hWnd, 0x00000000);
					setAffinity(g_hWnd, 0x00000011);
				}
			}
		}
		return true;
	}

	void DestroyHubWindow()
	{
		if (g_hWnd)
		{
			DestroyWindow(g_hWnd);
			g_hWnd = nullptr;
		}
		if (g_className[0])
		{
			UnregisterClassW(g_className, g_wc.hInstance);
			g_className[0] = L'\0';
		}
		if (g_wc.hbrBackground)
		{
			DeleteObject(g_wc.hbrBackground);
			g_wc.hbrBackground = nullptr;
		}
	}

	bool Pump()
	{
		MSG msg{};
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
			if (msg.message == WM_QUIT)
				g_requestClose = true;
		}
		return !g_requestClose;
	}
}

namespace FrameWork::WaitingWindow
{
	bool RunHub()
	{
		g_requestClose = false;
		g_readyToLoad = false;
		g_page = Page::Login;
		g_errorBuf[0] = '\0';
		g_keyBuf[0] = '\0';
		g_userLabel = "Guest";

#if defined(LICENSE_AUTH) && LICENSE_AUTH
		g_hwid = Cheat::GetHwidHash();
		std::string saved;
		if (Cheat::LoadSavedLicenseKey(saved))
			strncpy_s(g_keyBuf, saved.c_str(), _TRUNCATE);
#else
		g_hwid = "DEV";
		strncpy_s(g_keyBuf, "HZ-DEV", _TRUNCATE);
		g_userLabel = "Developer";
		g_page = Page::Home;
#endif
		HUB_DBG("RunHub start page=%s", g_page == Page::Home ? "home" : "login");

		if (!CreateHubWindow())
		{
			HUB_DBG("CreateHubWindow failed err=%lu", GetLastError());
			MessageBoxA(nullptr, "Could not create the HZ window.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONERROR);
			DestroyHubWindow();
			return false;
		}
		if (!CreateDevice())
		{
			HUB_DBG("CreateDevice failed");
			CleanupDevice();
			DestroyHubWindow();
			MessageBoxA(nullptr, "DirectX failed to create the HZ window.\nUpdate your GPU drivers and try again.", BRAND_MSG_BOX_TITLE, MB_OK | MB_ICONERROR);
			return false;
		}

		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0.f;
		style.WindowBorderSize = 0.f;
		style.WindowPadding = ImVec2{ 0.f, 0.f };
		style.WindowShadowSize = 0.f;
		style.Colors[ImGuiCol_WindowShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);
		style.FrameRounding = 8.f;
		style.FrameBorderSize = 1.f;
		style.FramePadding = ImVec2{ 12.f, 9.f };
		ImGui_ImplWin32_Init(g_hWnd);
		ImGui_ImplDX11_Init(g_device, g_context);
		g_imguiReady = true;

		while (!g_requestClose && !g_readyToLoad)
		{
			if (!Pump())
				break;
			RenderFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(16));
		}

		g_imguiReady = false;
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		CleanupDevice();
		DestroyHubWindow();

		return g_readyToLoad && !g_requestClose;
	}
}
