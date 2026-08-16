#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <memory>
// Use ImGui from the project's ImGui folder
#include "../../ImGui/imgui.h"
#include "../../ImGui/imgui_internal.h"
#include "../../ImGui/imgui_impl_dx11.h"
#include "../../ImGui/imgui_impl_win32.h"
#include <vector>
#include <string>
#include <d3d11.h>
#include <d3dx11tex.h>
#include <functional>
#include "unicodes.hpp"
#include "language.h"
#include <any>
#include <Windows.h>
#include <wininet.h>
#include <stdexcept>
#include "types.h"
#include "style.h"
#include "font.h"
#include "widgets.h"
#include "../../thirdparty/include/animations.hpp"
#include "draw.h"
#include "search.h"
#include "../../thirdparty/include/esppreview.h"
#include <d3dcompiler.h>
#include <thread>
#include "../../thirdparty/include/shadertoy.h"

#pragma comment( lib, "wininet.lib" )

#define ds * g_style->dpi_scale

class c_slate {
public:
	ID3D11Device* m_device; ID3D11DeviceContext* m_ctx; IDXGISwapChain* m_swapchain;

	float sidebar_anim;

	void initialize_fonts( );
	void initialize( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain );
	void draw( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain );
};

inline auto slate = std::make_unique< c_slate >( );

extern c_image logo;
