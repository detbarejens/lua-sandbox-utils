#include "Overlay.hpp"
#include "../Utils/DebugLog.hpp"
#include "../Utils/BrandPaths.hpp"
#include "../Definations/Brand.hpp"
#include "../Definations/Variables.hpp"
#include "../ImGui/imgui.h"

#include <cstring>
#include <shellapi.h>
#include <tlhelp32.h>
#include <dxgi.h>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "dxgi.lib")

#define DBG(fmt, ...) MELLO_DBG("[Overlay] " fmt, ##__VA_ARGS__)

struct WndRECT : public RECT
{
	int Width() { return right - left; }
	int Height() { return bottom - top; }
};

static inline std::function<void(HWND, UINT, WPARAM, LPARAM)> pWindowProc;

typedef HWND(WINAPI* CreateWindowInBand)(_In_ DWORD dwExStyle, _In_opt_ ATOM atom, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam, DWORD band);

bool bSettuped = false;
bool bInitialized = false;
bool bDeviceInitialized;
bool bRenderTargetInitialized;

HWND hWindow;
WNDCLASSEX WindowClass;
static wchar_t s_OverlayClassName[32] = {};
static ATOM s_OverlayClassAtom = 0;
inline HWND hTargetWindow = nullptr;
WndRECT wTargetWindowRect;
DWORD sTargetPid;

ID3D11Device* ID3dDevice;
ID3D11DeviceContext* ID3dDeviceContext;
IDXGISwapChain* ID3dSwapChain;
ID3D11RenderTargetView* ID3dRenderTargetView;

using SetWindowDisplayAffinityFn = BOOL(WINAPI*)(HWND, DWORD);

static SetWindowDisplayAffinityFn ResolveSetDisplayAffinity()
{
	static SetWindowDisplayAffinityFn fn = nullptr;
	static bool tried = false;
	if (tried)
		return fn;
	tried = true;

	wchar_t sysDir[MAX_PATH]{};
	HMODULE user32 = nullptr;
	if (GetSystemDirectoryW(sysDir, MAX_PATH))
	{
		wchar_t path[MAX_PATH]{};
		swprintf_s(path, L"%s\\user32.dll", sysDir);
		user32 = GetModuleHandleW(path);
		if (!user32)
			user32 = LoadLibraryW(path);
	}
	if (!user32)
		user32 = GetModuleHandleW(L"user32.dll");
	if (!user32)
		user32 = LoadLibraryW(L"user32.dll");
	if (user32)
		fn = reinterpret_cast<SetWindowDisplayAffinityFn>(GetProcAddress(user32, "SetWindowDisplayAffinity"));

	return fn;
}

static const wchar_t* NvidiaDxgiValueName()
{
	return L"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}";
}

static const wchar_t* const kNvidiaDxgiPaths[] = {
	L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS",
	L"SOFTWARE\\WOW6432Node\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS",
	L"SOFTWARE\\NVIDIA Corporation\\Global\\ShadowPlay\\FTS",
	L"SOFTWARE\\NVIDIA Corporation\\Global\\FTS",
};

static bool DeleteRegValue(HKEY root, const wchar_t* path, const wchar_t* name)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(root, path, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
		return false;
	const LSTATUS err = RegDeleteValueW(key, name);
	RegCloseKey(key);
	return err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND;
}

static bool HkLmNvidiaDxgiForced()
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS",
		0, KEY_READ, &key) != ERROR_SUCCESS)
		return false;

	DWORD current = 0;
	DWORD size = sizeof(current);
	DWORD type = 0;
	const bool ok = RegQueryValueExW(key, NvidiaDxgiValueName(), nullptr, &type,
		reinterpret_cast<LPBYTE>(&current), &size) == ERROR_SUCCESS
		&& type == REG_DWORD
		&& current == 0x24;
	RegCloseKey(key);
	return ok;
}

static void MarkNvidiaDxgiCleared()
{
	const std::string stampDir = BrandPaths::GetDataRoot();
	if (stampDir.empty())
		return;

	const std::string asked = stampDir + "nvidia-dxgi.asked";
	DeleteFileA(asked.c_str());

	const std::string stampPath = stampDir + "nvidia-dxgi.cleared";
	HANDLE stamp = CreateFileA(stampPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (stamp != INVALID_HANDLE_VALUE)
		CloseHandle(stamp);
}

static bool WasNvidiaDxgiCleared()
{
	const std::string stampDir = BrandPaths::GetDataRoot();
	if (stampDir.empty())
		return false;
	const std::string stampPath = stampDir + "nvidia-dxgi.cleared";
	return GetFileAttributesA(stampPath.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static void RestoreNvidiaCapture()
{
	static bool once = false;
	if (once)
		return;
	once = true;

	const wchar_t* valueName = NvidiaDxgiValueName();
	for (const wchar_t* path : kNvidiaDxgiPaths)
	{
		DeleteRegValue(HKEY_CURRENT_USER, path, valueName);
		DeleteRegValue(HKEY_LOCAL_MACHINE, path, valueName);
	}

	if (!HkLmNvidiaDxgiForced())
	{
		MarkNvidiaDxgiCleared();
		return;
	}

	if (WasNvidiaDxgiCleared())
		return;

	SHELLEXECUTEINFOW exec{};
	exec.cbSize = sizeof(exec);
	exec.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	exec.lpVerb = L"runas";
	exec.lpFile = L"cmd.exe";
	exec.lpParameters =
		L"/c reg delete \"HKLM\\SOFTWARE\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS\" /v \"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}\" /f"
		L" & reg delete \"HKLM\\SOFTWARE\\WOW6432Node\\NVIDIA Corporation\\Global\\NvApp\\ShadowPlay\\FTS\" /v \"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}\" /f"
		L" & reg delete \"HKLM\\SOFTWARE\\NVIDIA Corporation\\Global\\ShadowPlay\\FTS\" /v \"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}\" /f"
		L" & reg delete \"HKLM\\SOFTWARE\\NVIDIA Corporation\\Global\\FTS\" /v \"{497B8458-4244-4EE6-BFEA-F3D2BA294F21}\" /f";
	exec.nShow = SW_HIDE;

	if (!ShellExecuteExW(&exec))
		return;

	if (exec.hProcess)
	{
		WaitForSingleObject(exec.hProcess, 20000);
		CloseHandle(exec.hProcess);
	}

	if (!HkLmNvidiaDxgiForced())
		MarkNvidiaDxgiCleared();
}

static const wchar_t* NvidiaCapsPath()
{
	return L"SOFTWARE\\NVIDIA Corporation\\Global\\ShadowPlay\\NVSPCAPS";
}

static const wchar_t* const kNvidiaDwmValueNames[] = {
	L"DwmEnabled",
	L"DwmEnabledUser",
	L"DwmDvrEnabledV1",
};

static std::string NvidiaDwmBackupPath()
{
	const std::string stampDir = BrandPaths::GetDataRoot();
	if (stampDir.empty())
		return {};
	return stampDir + "nvidia-dwm.bak";
}

static bool ReadRegBinaryDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD* out)
{
	HKEY key = nullptr;
	if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
		return false;

	BYTE buf[8]{};
	DWORD size = sizeof(buf);
	DWORD type = 0;
	const LSTATUS err = RegQueryValueExW(key, name, nullptr, &type, buf, &size);
	RegCloseKey(key);
	if (err != ERROR_SUCCESS || size == 0)
		return false;

	DWORD value = 0;
	if (size >= 4)
		memcpy(&value, buf, sizeof(value));
	else
		value = buf[0];

	*out = value;
	return true;
}

static bool WriteRegBinaryDword(HKEY root, const wchar_t* path, const wchar_t* name, DWORD value)
{
	HKEY key = nullptr;
	if (RegCreateKeyExW(root, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
		return false;

	BYTE buf[4] = {
		static_cast<BYTE>(value & 0xFF),
		static_cast<BYTE>((value >> 8) & 0xFF),
		static_cast<BYTE>((value >> 16) & 0xFF),
		static_cast<BYTE>((value >> 24) & 0xFF),
	};
	const LSTATUS err = RegSetValueExW(key, name, 0, REG_BINARY, buf, 4);
	RegCloseKey(key);
	return err == ERROR_SUCCESS;
}

static bool LoadNvidiaDwmBackup(DWORD values[3])
{
	const std::string path = NvidiaDwmBackupPath();
	if (path.empty())
		return false;

	HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return false;

	DWORD read = 0;
	const BOOL ok = ReadFile(file, values, sizeof(DWORD) * 3, &read, nullptr);
	CloseHandle(file);
	return ok && read == sizeof(DWORD) * 3;
}

static void BackupNvidiaDwmIfNeeded()
{
	DWORD existing[3]{};
	if (LoadNvidiaDwmBackup(existing))
		return;

	const std::string path = NvidiaDwmBackupPath();
	if (path.empty())
		return;

	DWORD values[3]{};
	for (int i = 0; i < 3; ++i)
	{
		DWORD current = 0;
		if (ReadRegBinaryDword(HKEY_CURRENT_USER, NvidiaCapsPath(), kNvidiaDwmValueNames[i], &current))
			values[i] = current;
		else
			values[i] = 1;
	}

	HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;

	DWORD written = 0;
	WriteFile(file, values, sizeof(values), &written, nullptr);
	CloseHandle(file);
}

static bool SetNvidiaDwmValues(const DWORD values[3])
{
	bool changed = false;
	for (int i = 0; i < 3; ++i)
	{
		DWORD current = 0xFFFFFFFFu;
		const bool have = ReadRegBinaryDword(HKEY_CURRENT_USER, NvidiaCapsPath(), kNvidiaDwmValueNames[i], &current);
		if (have && current == values[i])
			continue;

		if (WriteRegBinaryDword(HKEY_CURRENT_USER, NvidiaCapsPath(), kNvidiaDwmValueNames[i], values[i]))
			changed = true;
	}
	return changed;
}

static bool NvidiaDesktopCaptureIsOff()
{
	DWORD dwm = 1;
	DWORD user = 1;
	ReadRegBinaryDword(HKEY_CURRENT_USER, NvidiaCapsPath(), L"DwmEnabled", &dwm);
	ReadRegBinaryDword(HKEY_CURRENT_USER, NvidiaCapsPath(), L"DwmEnabledUser", &user);
	return dwm == 0 && user == 0;
}

static DWORD WINAPI RewriteNvidiaDwmOffThread(LPVOID)
{
	Sleep(1500);
	const DWORD again[3] = { 0, 0, 0 };
	SetNvidiaDwmValues(again);
	return 0;
}

static bool s_AllowNvidiaOverlayRestart = false;

static void RestartNvidiaOverlayProcess()
{
	if (!s_AllowNvidiaOverlayRestart)
		return;

	static const wchar_t* kNames[] = {
		L"NVIDIA Overlay.exe",
		L"NVIDIA Share.exe",
	};

	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
		return;

	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (!Process32FirstW(snap, &entry))
	{
		CloseHandle(snap);
		return;
	}

	do
	{
		bool match = false;
		for (const wchar_t* name : kNames)
		{
			if (_wcsicmp(entry.szExeFile, name) == 0)
			{
				match = true;
				break;
			}
		}
		if (!match)
			continue;

		HANDLE process = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
		if (!process)
			continue;

		TerminateProcess(process, 0);
		CloseHandle(process);
	} while (Process32NextW(snap, &entry));

	CloseHandle(snap);
}

static void SyncNvidiaStreamProof(bool streamProof, bool force = false)
{
	static bool haveLast = false;
	static bool lastStreamProof = false;
	static ULONGLONG lastCheck = 0;

	const ULONGLONG now = GetTickCount64();
	const bool stateChanged = force || !haveLast || lastStreamProof != streamProof;
	if (!stateChanged && (now - lastCheck) < 4000)
		return;

	lastCheck = now;
	lastStreamProof = streamProof;
	haveLast = true;

	if (streamProof)
	{
		BackupNvidiaDwmIfNeeded();
		const DWORD off[3] = { 0, 0, 0 };
		const bool wrote = SetNvidiaDwmValues(off);
		if (wrote || !NvidiaDesktopCaptureIsOff())
			SetNvidiaDwmValues(off);

		if (stateChanged && s_AllowNvidiaOverlayRestart)
		{
			RestartNvidiaOverlayProcess();
			HANDLE delayed = CreateThread(nullptr, 0, RewriteNvidiaDwmOffThread, nullptr, 0, nullptr);
			if (delayed)
				CloseHandle(delayed);
			DBG("NVIDIA Instant Replay set to game capture (desktop capture off)");
		}
	}
	else
	{
		DWORD backup[3] = { 1, 1, 1 };
		if (!LoadNvidiaDwmBackup(backup))
		{
			backup[0] = 1;
			backup[1] = 1;
			backup[2] = 1;
		}

		if (SetNvidiaDwmValues(backup) && stateChanged && s_AllowNvidiaOverlayRestart)
			DBG("NVIDIA desktop capture restored");
	}
}

static void BindSwapChainToOverlayWindow()
{
	if (!ID3dDevice || !ID3dSwapChain || !hWindow)
		return;

	ID3dSwapChain->SetFullscreenState(FALSE, nullptr);

	IDXGIDevice* dxgiDevice = nullptr;
	if (FAILED(ID3dDevice->QueryInterface(IID_PPV_ARGS(&dxgiDevice))) || !dxgiDevice)
		return;

	IDXGIAdapter* adapter = nullptr;
	if (FAILED(dxgiDevice->GetAdapter(&adapter)) || !adapter)
	{
		dxgiDevice->Release();
		return;
	}

	IDXGIFactory* factory = nullptr;
	if (SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))) && factory)
	{
		factory->MakeWindowAssociation(hWindow, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
		factory->Release();
	}

	adapter->Release();
	dxgiDevice->Release();
}

void CreateDeviceD3D()
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	ZeroMemory(&SwapChainDesc, sizeof(SwapChainDesc));
	SwapChainDesc.BufferDesc.Width = 0;
	SwapChainDesc.BufferDesc.Height = 0;
	SwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	SwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.SampleDesc.Quality = 0;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = hWindow;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	SwapChainDesc.Flags = 0;

	D3D_FEATURE_LEVEL FeatureLevel;
	const D3D_FEATURE_LEVEL FeatureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, FeatureLevelArray, 2, D3D11_SDK_VERSION, &SwapChainDesc, &ID3dSwapChain, &ID3dDevice, &FeatureLevel, &ID3dDeviceContext);
	if (FAILED(hr)) {
		DBG("D3D11CreateDeviceAndSwapChain failed: 0x%X", hr);
		return;
	}

	BindSwapChainToOverlayWindow();
	bDeviceInitialized = true;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_ERASEBKGND)
		return 1;

	if (pWindowProc)
		pWindowProc(hWnd, uMsg, wParam, lParam);
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

namespace FrameWork
{
static bool GetTargetScreenRect(HWND target, RECT& rect)
{
	return target && IsWindow(target) && GetWindowRect(target, &rect);
}

void Overlay::Setup(DWORD TargetPid)
{
	sTargetPid = TargetPid;
	DBG("Setup: TargetPid=%d", TargetPid);

	hTargetWindow = Memory::FindGameWindow(TargetPid);
	if (!hTargetWindow)
		hTargetWindow = FindWindowA("grcWindow", nullptr);
	if (!hTargetWindow)
		hTargetWindow = FindWindowA("grcWindowEx", nullptr);
	DBG("FindGameWindow: %p", hTargetWindow);

	if (hTargetWindow && GetTargetScreenRect(hTargetWindow, wTargetWindowRect)
		&& wTargetWindowRect.Width() >= 200 && wTargetWindowRect.Height() >= 150)
	{
		DBG("Target window rect: L=%d T=%d R=%d B=%d (%dx%d)",
			wTargetWindowRect.left, wTargetWindowRect.top, wTargetWindowRect.right, wTargetWindowRect.bottom,
			wTargetWindowRect.Width(), wTargetWindowRect.Height());
		bSettuped = true;
		DBG("Setup complete: bSettuped=true");
	}
	else {
		hTargetWindow = nullptr;
		bSettuped = false;
		DBG("Setup failed: valid FiveM grcWindow not found");
	}
}

void Overlay::Initialize()
{
	if (!bSettuped) {
		DBG("Initialize: not setup (bSettuped=false)");
		return;
	}

	if (!s_OverlayClassName[0])
		swprintf_s(s_OverlayClassName, BRAND_OVERLAY_CLASS_FMT, GetCurrentProcessId() & 0xFFFF);

	WindowClass.cbSize = sizeof(WindowClass);
	WindowClass.style = 0;
	WindowClass.lpfnWndProc = WindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = GetModuleHandle(NULL);
	WindowClass.hIcon = NULL;
	WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = s_OverlayClassName;
	WindowClass.hIconSm = NULL;

	if (!s_OverlayClassAtom)
	{
		s_OverlayClassAtom = SafeCall(RegisterClassEx)(&WindowClass);
		if (!s_OverlayClassAtom) {
			DBG("RegisterClassEx failed: %lu", GetLastError());
			return;
		}
	}

	hWindow = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
		reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(s_OverlayClassAtom)),
		L"",
		WS_POPUP,
		wTargetWindowRect.left, wTargetWindowRect.top, wTargetWindowRect.Width(), wTargetWindowRect.Height(),
		NULL, NULL, GetModuleHandle(NULL), NULL);
	if (!hWindow) {
		DBG("CreateWindowEx failed: %lu", GetLastError());
		return;
	}

	MARGINS Margins = { -1, -1, -1, -1 };
	HRESULT dwmResult = DwmExtendFrameIntoClientArea(hWindow, &Margins);
	if (FAILED(dwmResult)) {
		DBG("DwmExtendFrameIntoClientArea failed: 0x%X", dwmResult);
	}
	SafeCall(SetLayeredWindowAttributes)(hWindow, RGB(0, 0, 0), 255, LWA_ALPHA);
	s_AllowNvidiaOverlayRestart = false;
	RestoreNvidiaCapture();
	ApplyCaptureBypass();
	SafeCall(ShowWindow)(hWindow, SW_SHOWNOACTIVATE);
	SafeCall(SetWindowPos)(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	bInitialized = true;
	DBG("Window created successfully, initializing D3D...");
	dxInitialize();
	ApplyCaptureBypass();
}

	void Overlay::ShutDown()
	{
		SafeCall(DestroyWindow)(hWindow);
		if (s_OverlayClassAtom)
		{
			SafeCall(UnregisterClass)(reinterpret_cast<LPCWSTR>(static_cast<ULONG_PTR>(s_OverlayClassAtom)), WindowClass.hInstance);
			s_OverlayClassAtom = 0;
		}
		bInitialized = false; bSettuped = false;
	}

	void Overlay::UpdateWindowPos()
	{
		if (g_Options.General.SecondMonitor)
		{
			if (g_Options.General.MonitorIndex < 0)
				g_Options.General.MonitorIndex = 0;

			ApplySecondMonitorLayout();
			return;
		}

		RECT targetRect{};
		if (!GetTargetScreenRect(hTargetWindow, targetRect))
			return;

		static RECT lastSeen{};
		static int stableFrames = 0;
		if (lastSeen.left == targetRect.left && lastSeen.top == targetRect.top
			&& lastSeen.right == targetRect.right && lastSeen.bottom == targetRect.bottom)
		{
			++stableFrames;
		}
		else
		{
			lastSeen = targetRect;
			stableFrames = 0;
			return;
		}

		if (stableFrames < 8)
			return;

		RECT currentRect{};
		if (GetWindowRect(hWindow, &currentRect)
			&& currentRect.left == targetRect.left
			&& currentRect.top == targetRect.top
			&& (currentRect.right - currentRect.left) == (targetRect.right - targetRect.left)
			&& (currentRect.bottom - currentRect.top) == (targetRect.bottom - targetRect.top))
			return;

		MoveWindow(hWindow, targetRect.left, targetRect.top, targetRect.right - targetRect.left, targetRect.bottom - targetRect.top, FALSE);
		ResizeSwapChainToWindow();
		ApplyCaptureBypass();
	}

	void Overlay::SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion)
	{
		pWindowProc = Funtion;
	}

	void Overlay::dxInitialize()
	{
		CreateDeviceD3D();
		if (bDeviceInitialized)
			dxCreateRenderTarget();
	}

	void Overlay::dxRefresh()
	{
		if (!ID3dDeviceContext || !ID3dRenderTargetView)
			return;

		ID3dDeviceContext->OMSetRenderTargets(1, &ID3dRenderTargetView, nullptr);
		if (IsSecondMonitorEspActive())
		{
			static float BlackColor[4] = { 0.f, 0.f, 0.f, 1.f };
			ID3dDeviceContext->ClearRenderTargetView(ID3dRenderTargetView, BlackColor);
		}
		else
		{
			static float TransparentColor[4] = { 0, 0, 0, 0 };
			ID3dDeviceContext->ClearRenderTargetView(ID3dRenderTargetView, TransparentColor);
		}
	}

	void Overlay::ResizeSwapChainToWindow()
	{
		if (!hWindow || !ID3dSwapChain)
			return;

		RECT rc{};
		if (!GetClientRect(hWindow, &rc))
			return;

		const UINT width = static_cast<UINT>(rc.right - rc.left);
		const UINT height = static_cast<UINT>(rc.bottom - rc.top);
		if (width == 0 || height == 0)
			return;

		DXGI_SWAP_CHAIN_DESC desc{};
		if (FAILED(ID3dSwapChain->GetDesc(&desc)))
			return;

		if (desc.BufferDesc.Width == width && desc.BufferDesc.Height == height)
			return;

		dxCleanupRenderTarget();
		const HRESULT hr = ID3dSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		if (SUCCEEDED(hr))
			dxCreateRenderTarget();
		else
			DBG("ResizeBuffers failed: 0x%X", hr);
	}

	void Overlay::dxShutDown()
	{
		dxCleanupRenderTarget();
		dxCleanupDeviceD3D();
	}

void Overlay::dxCreateRenderTarget()
{
	DBG("dxCreateRenderTarget: creating render target view");
	ID3D11Texture2D* pBackBuffer;
	HRESULT hr = ID3dSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (FAILED(hr)) {
		DBG("GetBuffer failed: 0x%X", hr);
		return;
	}
	hr = ID3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &ID3dRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr)) {
		DBG("CreateRenderTargetView failed: 0x%X", hr);
		return;
	}
	bRenderTargetInitialized = true;
	DBG("dxCreateRenderTarget: success");
}

	void Overlay::dxCleanupRenderTarget()
	{
		if (ID3dRenderTargetView) { ID3dRenderTargetView->Release(); ID3dRenderTargetView = NULL; }
		bRenderTargetInitialized = false;
	}

	void Overlay::dxCleanupDeviceD3D()
	{
		if (ID3dSwapChain) { ID3dSwapChain->Release(); ID3dSwapChain = NULL; }
		if (ID3dDeviceContext) { ID3dDeviceContext->Release(); ID3dDeviceContext = NULL; }
		if (ID3dDevice) { ID3dDevice->Release(); ID3dDevice = NULL; }
		bDeviceInitialized = false;
	}

	void Overlay::ApplyCaptureBypass()
	{
		const bool exclude = g_Options.General.CaptureBypass;
		SyncNvidiaStreamProof(exclude);

		if (!hWindow || !IsWindow(hWindow))
			return;

		auto setFn = ResolveSetDisplayAffinity();
		if (!setFn)
			return;

		const DWORD wanted = exclude ? 0x00000011u : 0x00000000u;
		setFn(hWindow, wanted);
	}

	void Overlay::EnsureNvidiaCapturePath()
	{
		RestoreNvidiaCapture();
		s_AllowNvidiaOverlayRestart = true;
		SyncNvidiaStreamProof(g_Options.General.CaptureBypass, true);
		s_AllowNvidiaOverlayRestart = false;
		Sleep(750);
	}

	void Overlay::ApplyStreamProof(bool enabled)
	{
		g_Options.General.CaptureBypass = enabled;
		RestoreNvidiaCapture();
		s_AllowNvidiaOverlayRestart = true;
		SyncNvidiaStreamProof(enabled, true);
		s_AllowNvidiaOverlayRestart = false;

		if (!hWindow || !IsWindow(hWindow))
			return;

		auto setFn = ResolveSetDisplayAffinity();
		if (!setFn)
			return;

		setFn(hWindow, enabled ? 0x00000011u : 0x00000000u);
	}

	bool Overlay::IsSettuped() { return bSettuped; }
	bool Overlay::IsInitialized() { return bInitialized; }
	bool Overlay::IsRenderReady() { return bInitialized && bDeviceInitialized && bRenderTargetInitialized; }
	HWND Overlay::GetOverlayWindow() { return hWindow; }
	HWND Overlay::GetTargetWindow() { return hTargetWindow; }
	ID3D11Device* Overlay::dxGetDevice() { return ID3dDevice; }
	ID3D11DeviceContext* Overlay::dxGetDeviceContext() { return ID3dDeviceContext; }
	IDXGISwapChain* Overlay::dxGetSwapChain() { return ID3dSwapChain; }
	ID3D11RenderTargetView* Overlay::dxGetRenderTarget() { return ID3dRenderTargetView; }
}

static int sSelectedMonitorIndex = -1;

static std::vector<MONITORINFOEX> EnumerateMonitors()
{
	std::vector<MONITORINFOEX> result;
	EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR hMon, HDC, LPRECT, LPARAM lp) -> BOOL {
		auto* out = reinterpret_cast<std::vector<MONITORINFOEX>*>(lp);
		MONITORINFOEX mi{};
		mi.cbSize = sizeof(MONITORINFOEX);
		GetMonitorInfo(hMon, &mi);
		out->push_back(mi);
		return TRUE;
	}, reinterpret_cast<LPARAM>(&result));
	return result;
}

namespace FrameWork
{
	bool Overlay::IsSecondMonitorEspActive()
	{
		return g_Options.General.SecondMonitor && g_Options.General.MonitorIndex >= 0;
	}

	ImVec2 Overlay::GetGameViewportSize()
	{
		RECT targetRect{};
		if (GetTargetScreenRect(hTargetWindow, targetRect))
		{
			const float w = static_cast<float>(targetRect.right - targetRect.left);
			const float h = static_cast<float>(targetRect.bottom - targetRect.top);
			if (w > 1.f && h > 1.f)
				return ImVec2{ w, h };
		}

		return ImGui::GetIO().DisplaySize;
	}

	ImVec2 Overlay::GetProjectionSize()
	{
		if (IsSecondMonitorEspActive())
			return GetGameViewportSize();
		return ImGui::GetIO().DisplaySize;
	}

	ImVec2 Overlay::GetEspScale()
	{
		if (!IsSecondMonitorEspActive())
			return ImVec2{ 1.f, 1.f };

		const ImVec2 gameSize = GetGameViewportSize();
		const ImVec2 outSize = ImGui::GetIO().DisplaySize;
		if (gameSize.x <= 1.f || gameSize.y <= 1.f)
			return ImVec2{ 1.f, 1.f };

		return ImVec2{ outSize.x / gameSize.x, outSize.y / gameSize.y };
	}

	void Overlay::ApplySecondMonitorLayout()
	{
		if (!hWindow)
			return;

		auto monitors = EnumerateMonitors();
		const int index = g_Options.General.MonitorIndex;
		if (index < 0 || index >= static_cast<int>(monitors.size()))
			return;

		const RECT rc = monitors[index].rcMonitor;
		const int width = rc.right - rc.left;
		const int height = rc.bottom - rc.top;

		RECT currentRect{};
		if (GetWindowRect(hWindow, &currentRect)
			&& currentRect.left == rc.left
			&& currentRect.top == rc.top
			&& (currentRect.right - currentRect.left) == width
			&& (currentRect.bottom - currentRect.top) == height)
			return;

		MoveWindow(hWindow, rc.left, rc.top, width, height, FALSE);
		ResizeSwapChainToWindow();
		ApplyCaptureBypass();
	}

	std::vector<std::string> Overlay::GetMonitorNames()
	{
		std::vector<std::string> names;
		auto monitors = EnumerateMonitors();
		for (size_t i = 0; i < monitors.size(); ++i)
		{
			char deviceName[128];
			WideCharToMultiByte(CP_UTF8, 0, monitors[i].szDevice, -1, deviceName, sizeof(deviceName), NULL, NULL);
			int width  = monitors[i].rcMonitor.right  - monitors[i].rcMonitor.left;
			int height = monitors[i].rcMonitor.bottom - monitors[i].rcMonitor.top;
			std::string name = deviceName;
			name += " - " + std::to_string(width) + "x" + std::to_string(height);
			if (monitors[i].dwFlags & MONITORINFOF_PRIMARY)
				name += " (Primary)";
			names.push_back(name);
		}
		return names;
	}

	void Overlay::SetMonitorIndex(int index)
	{
		sSelectedMonitorIndex = index;
		g_Options.General.MonitorIndex = index;
		if (IsSecondMonitorEspActive())
			ApplySecondMonitorLayout();
	}
}
