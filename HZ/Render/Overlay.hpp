#pragma once

#include <Windows.h>
#include <functional>
#include <vector>
#include <string>

#include <d3d11.h>
#include <d3dx11.h>

#include "../FiveM-External.hpp"

namespace FrameWork
{
	namespace Overlay
	{
		inline HWND hTargetWindow;

		void Setup(DWORD TargetPid);
		void Initialize();
		void ShutDown();

		void UpdateWindowPos();
		void ApplyCaptureBypass();
		void EnsureNvidiaCapturePath();
		void ApplyStreamProof(bool enabled);
		void SetupWindowProcHook(std::function<void(HWND, UINT, WPARAM, LPARAM)> Funtion);

		// Monitor / second-screen ESP
		std::vector<std::string> GetMonitorNames();
		void SetMonitorIndex(int index);
		bool IsSecondMonitorEspActive();
		ImVec2 GetGameViewportSize();
		ImVec2 GetProjectionSize();
		ImVec2 GetEspScale();
		void ApplySecondMonitorLayout();

		bool IsSettuped();
		bool IsInitialized();
		bool IsRenderReady();
		HWND GetOverlayWindow();
		HWND GetTargetWindow();

		void dxInitialize();
		void dxRefresh();
		void dxShutDown();
		void ResizeSwapChainToWindow();

		void dxCreateRenderTarget();
		void dxCleanupRenderTarget();
		void dxCleanupDeviceD3D();

		ID3D11Device* dxGetDevice();
		ID3D11DeviceContext* dxGetDeviceContext();
		IDXGISwapChain* dxGetSwapChain();
		ID3D11RenderTargetView* dxGetRenderTarget();
	}
}