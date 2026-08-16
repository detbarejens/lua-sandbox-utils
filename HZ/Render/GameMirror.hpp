#pragma once

#include <Windows.h>
#include <d3d11.h>

namespace FrameWork
{
	namespace GameMirror
	{
		void Initialize( ID3D11Device* device, ID3D11DeviceContext* context );
		void Shutdown( );
		bool Update( HWND gameWindow, HWND overlayWindow = nullptr );
		bool IsReady( );
		ID3D11ShaderResourceView* GetSrv( );
	}
}
