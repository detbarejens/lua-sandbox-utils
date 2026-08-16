#pragma once

#include "../FiveM-External.hpp"
#include "../Definations/Cheat.hpp"

#include <d3d11.h>
#include <d3dx11.h>

namespace FrameWork
{
	// Simple notification structure
	struct ConfigNotification {
		std::string message;
		float lifeTime;
		float fadeAnim;
	};

	class Interface
	{
	public:
		Interface() { }
		Interface(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext) { Initialize(Window, TargetWindow, Device, DeviceContext); }
		~Interface() { ShutDown(); }

		void Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext);
		void UpdateStyle();

		void RenderGui();
		void RenderLoadingScreen();
		void RenderAuthScreen();

		void WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		void HandleMenuKey();
		void SetMenuOpen(bool open);
		void RefreshWindowStyle();

		void ShutDown();

		bool GetMenuOpen() { return bIsMenuOpen; }
		
		std::string GetKeyName(int key);
		void RenderKeybindList();
		void RenderWatermark();
		void RenderSessionInfo();
		
		// Notification system
		std::vector<ConfigNotification> configNotifications;
		void AddConfigNotification(const std::string& message);
		void RenderConfigNotifications();

	public:
		int DelayedMenuOpenFrames = 0;
		bool bShowLoading = false;
		float fLoadingProgress = 0.f;
		bool bNotificationSent = false;
		bool bShowAuth = false;
		char DiscordIDBuffer[64] = "";
#if defined(LICENSE_AUTH) && LICENSE_AUTH
		char LicenseKeyBuffer[256] = "";
		char AuthErrorBuffer[256] = "";
		char HwidDisplayBuffer[72] = "";
		bool AuthBusy = false;
#endif
		ID3D11ShaderResourceView* DiscordAvatarTexture = nullptr;
		float fEspPreviewAnim = 0.f; // Animation for ESP preview panel
		
		// Keybind list
		struct KeybindState {
			std::string name;
			std::string key;
			bool active;
			float activeAnim;
		};
		std::vector<KeybindState> keybindStates;

	private:
		HWND hWindow;
		HWND hTargetWindow;
		ID3D11Device* IDevice;

		bool bIsMenuOpen = false;

		void ApplyMenuWindowStyle();

	public:
		UINT ResizeWidht = 0;
		UINT ResizeHeight = 0;
	};
}