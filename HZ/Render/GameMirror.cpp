#include "GameMirror.hpp"

#include <chrono>
#include <dwmapi.h>
#include <vector>

#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

namespace
{
	ID3D11Device* g_device = nullptr;
	ID3D11DeviceContext* g_context = nullptr;

	HDC g_memDc = nullptr;
	HBITMAP g_bitmap = nullptr;
	HGDIOBJ g_oldBitmap = nullptr;
	int g_captureW = 0;
	int g_captureH = 0;

	std::vector<BYTE> g_pixels;

	ID3D11Texture2D* g_textures[2] = { nullptr, nullptr };
	ID3D11ShaderResourceView* g_srvs[2] = { nullptr, nullptr };
	int g_frontIndex = 0;
	int g_texW = 0;
	int g_texH = 0;

	bool RectsOverlap( const RECT& a, const RECT& b )
	{
		return a.left < b.right && a.right > b.left && a.top < b.bottom && a.bottom > b.top;
	}

	void ReleaseCaptureBuffers( )
	{
		if ( g_memDc && g_oldBitmap )
		{
			SelectObject( g_memDc, g_oldBitmap );
			g_oldBitmap = nullptr;
		}
		if ( g_bitmap )
		{
			DeleteObject( g_bitmap );
			g_bitmap = nullptr;
		}
		if ( g_memDc )
		{
			DeleteDC( g_memDc );
			g_memDc = nullptr;
		}
		g_captureW = 0;
		g_captureH = 0;
		g_pixels.clear( );
	}

	void ReleaseTextures( )
	{
		for ( int i = 0; i < 2; ++i )
		{
			if ( g_srvs[i] )
			{
				g_srvs[i]->Release( );
				g_srvs[i] = nullptr;
			}
			if ( g_textures[i] )
			{
				g_textures[i]->Release( );
				g_textures[i] = nullptr;
			}
		}
		g_texW = 0;
		g_texH = 0;
		g_frontIndex = 0;
	}

	bool EnsureCaptureBuffers( int width, int height )
	{
		if ( width <= 0 || height <= 0 )
			return false;

		if ( width == g_captureW && height == g_captureH && g_memDc && g_bitmap )
			return true;

		ReleaseCaptureBuffers( );
		g_captureW = width;
		g_captureH = height;
		g_pixels.resize( static_cast<size_t>( width ) * static_cast<size_t>( height ) * 4 );

		HDC screenDc = GetDC( nullptr );
		if ( !screenDc )
			return false;

		g_memDc = CreateCompatibleDC( screenDc );
		g_bitmap = CreateCompatibleBitmap( screenDc, width, height );
		ReleaseDC( nullptr, screenDc );

		if ( !g_memDc || !g_bitmap )
		{
			ReleaseCaptureBuffers( );
			return false;
		}

		g_oldBitmap = SelectObject( g_memDc, g_bitmap );
		return true;
	}

	bool EnsureGpuTextures( int width, int height )
	{
		if ( !g_device || width <= 0 || height <= 0 )
			return false;

		if ( width == g_texW && height == g_texH && g_textures[0] && g_textures[1] )
			return true;

		ReleaseTextures( );
		g_texW = width;
		g_texH = height;

		for ( int i = 0; i < 2; ++i )
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = static_cast<UINT>( width );
			desc.Height = static_cast<UINT>( height );
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			if ( FAILED( g_device->CreateTexture2D( &desc, nullptr, &g_textures[i] ) ) )
			{
				ReleaseTextures( );
				return false;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MipLevels = 1;

			if ( FAILED( g_device->CreateShaderResourceView( g_textures[i], &srvDesc, &g_srvs[i] ) ) )
			{
				ReleaseTextures( );
				return false;
			}
		}

		g_frontIndex = 0;
		return true;
	}

	bool ReadCapturedPixels( int width, int height )
	{
		BITMAPINFO info{};
		info.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
		info.bmiHeader.biWidth = width;
		info.bmiHeader.biHeight = -height;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;

		const int lines = GetDIBits(
			g_memDc, g_bitmap, 0, height,
			g_pixels.data( ), &info, DIB_RGB_COLORS
		);
		return lines == height;
	}

	bool CaptureGameWindow( HWND gameWindow, HWND overlayWindow, const RECT& rect )
	{
		const int width = rect.right - rect.left;
		const int height = rect.bottom - rect.top;
		if ( !EnsureCaptureBuffers( width, height ) )
			return false;

		RECT overlayRect{};
		const bool overlayCoversCapture = overlayWindow && IsWindow( overlayWindow ) && IsWindowVisible( overlayWindow )
			&& GetWindowRect( overlayWindow, &overlayRect ) && RectsOverlap( rect, overlayRect );

		BOOL captured = FALSE;

		// Read pixels already on screen — does not force the game to redraw (PrintWindow does).
		if ( !overlayCoversCapture )
		{
			HDC screenDc = GetDC( nullptr );
			if ( screenDc )
			{
				captured = BitBlt(
					g_memDc, 0, 0, width, height,
					screenDc, rect.left, rect.top,
					SRCCOPY
				);
				ReleaseDC( nullptr, screenDc );
			}
		}

		// Fallback only when screen capture fails (e.g. exclusive fullscreen).
		if ( !captured )
			captured = PrintWindow( gameWindow, g_memDc, PW_RENDERFULLCONTENT );

		return captured && ReadCapturedPixels( width, height );
	}

	bool UploadCapturedFrame( )
	{
		if ( !g_context || g_captureW <= 0 || g_captureH <= 0 || g_pixels.empty( ) )
			return false;

		if ( !EnsureGpuTextures( g_captureW, g_captureH ) )
			return false;

		const int backIndex = 1 - g_frontIndex;
		g_context->UpdateSubresource(
			g_textures[backIndex], 0, nullptr,
			g_pixels.data( ),
			static_cast<UINT>( g_captureW * 4 ), 0
		);
		g_frontIndex = backIndex;
		return true;
	}
}

namespace FrameWork
{
	namespace GameMirror
	{
		void Initialize( ID3D11Device* device, ID3D11DeviceContext* context )
		{
			g_device = device;
			g_context = context;
		}

		void Shutdown( )
		{
			ReleaseCaptureBuffers( );
			ReleaseTextures( );
			g_device = nullptr;
			g_context = nullptr;
		}

		bool Update( HWND gameWindow, HWND overlayWindow )
		{
			if ( !g_device || !gameWindow || !IsWindow( gameWindow ) )
				return IsReady( );

			static auto lastCapture = std::chrono::steady_clock::now( );
			const auto now = std::chrono::steady_clock::now( );
			if ( now - lastCapture < std::chrono::milliseconds( 16 ) )
				return IsReady( );

			RECT rect{};
			if ( !GetWindowRect( gameWindow, &rect ) )
				return IsReady( );

			const int width = rect.right - rect.left;
			const int height = rect.bottom - rect.top;
			if ( width <= 0 || height <= 0 )
				return IsReady( );

			if ( CaptureGameWindow( gameWindow, overlayWindow, rect ) && UploadCapturedFrame( ) )
			{
				lastCapture = now;
				return true;
			}

			return IsReady( );
		}

		bool IsReady( )
		{
			return g_srvs[g_frontIndex] != nullptr;
		}

		ID3D11ShaderResourceView* GetSrv( )
		{
			return g_srvs[g_frontIndex];
		}
	}
}
