#pragma once

#include "../../FiveM-External.hpp"
#include "../../ImGui/imgui.h"

#include <d3d11.h>
#include <d3dx11.h>

namespace FrameWork
{
	namespace Assets
	{
		inline ImFont* InterBold = nullptr;
		inline ImFont* InterBold12 = nullptr;
		inline ImFont* InterBold10 = nullptr; // Fonte menor para ESP
		inline ImFont* InterMedium10 = nullptr; // Fonte mais bonita para ESP
		inline ImFont* InterBlack14 = nullptr;
		inline ImFont* InterBlack18 = nullptr;
		inline ImFont* InterMedium12 = nullptr;
		inline ImFont* InterMedium14 = nullptr;
		inline ImFont* InterMedium16 = nullptr;
		inline ImFont* FontAwesomeRegular = nullptr;
		inline ImFont* FontAwesomeSolid = nullptr;
		inline ImFont* FontAwesomeSolid14 = nullptr;
		inline ImFont* FontAwesomeBrands = nullptr;

		inline ImFont* InterBlack = nullptr;
		inline ImFont* InterExtraBold = nullptr;
		inline ImFont* InterExtraLight = nullptr;
		inline ImFont* InterLight = nullptr;
		inline ImFont* InterMedium = nullptr;
		inline ImFont* InterRegular = nullptr;
		inline ImFont* InterSemiBold = nullptr;
		inline ImFont* InterThin = nullptr;

		inline ID3D11ShaderResourceView* Logo = nullptr;

		void Initialize(ID3D11Device* Device);
	}
}