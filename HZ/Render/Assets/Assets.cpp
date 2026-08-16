#include "Assets.hpp"
#include "../../Definations/Brand.hpp"

#include "Data/FontAwesome.hpp"
#include "Data/FontData.hpp"
#include "Data/ImageData.hpp"

namespace FrameWork
{
    void Assets::Initialize(ID3D11Device* Device)
    {
        static bool s_loaded = false;
        if (s_loaded || !Device)
            return;
        s_loaded = true;

        ImGuiIO& io = ImGui::GetIO();

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
        InterBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 16);
        InterBold12 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 12);
        InterBold10 = InterBold12;
        InterMedium10 = InterBold12;
        InterBlack14 = InterBold;
        InterBlack18 = InterBold;
        InterMedium12 = InterBold12;
        InterMedium14 = InterBold;
        InterMedium16 = InterBold;
        InterBlack = InterBold;
        InterExtraBold = InterBold;
        InterExtraLight = InterBold12;
        InterLight = InterBold12;
        InterMedium = InterBold;
        InterRegular = InterBold;
        InterSemiBold = InterBold;
        InterThin = InterBold12;
        FontAwesomeRegular = nullptr;
        FontAwesomeSolid = nullptr;
        FontAwesomeSolid14 = nullptr;
        FontAwesomeBrands = nullptr;
#else
        InterBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 16);
        InterBold12 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 12);
        InterBold10 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBold_compressed_data, InterBold_compressed_size, 10);
        InterMedium10 = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 10);
        InterBlack14 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 14);
        InterBlack18 = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 16);
        InterMedium12 = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 12);
        InterMedium14 = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 14);
        InterMedium16 = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 16);

        InterBlack = io.Fonts->AddFontFromMemoryCompressedTTF(InterBlack_compressed_data, InterBlack_compressed_size, 14);
        InterExtraBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraBold_compressed_data, InterExtraBold_compressed_size, 14);
        InterExtraLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterExtraLight_compressed_data, InterExtraLight_compressed_size, 14);
        InterLight = io.Fonts->AddFontFromMemoryCompressedTTF(InterLight_compressed_data, InterLight_compressed_size, 16);
        InterMedium = io.Fonts->AddFontFromMemoryCompressedTTF(InterMedium_compressed_data, InterMedium_compressed_size, 16);
        InterRegular = io.Fonts->AddFontFromMemoryCompressedTTF(InterRegular_compressed_data, InterRegular_compressed_size, 16);
        InterSemiBold = io.Fonts->AddFontFromMemoryCompressedTTF(InterSemiBold_compressed_data, InterSemiBold_compressed_size, 15);
        InterThin = io.Fonts->AddFontFromMemoryCompressedTTF(InterThin_compressed_data, InterThin_compressed_size, 14);

        ImFontConfig FontAwesomeConfig;
        static const ImWchar FontAwesomeRanges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
        static const ImWchar FontAwesomeRangesBrands[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };

        FontAwesomeRegular = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Regular_compressed_data, FontAwesome6Regular_compressed_size, 17.f, &FontAwesomeConfig, FontAwesomeRanges);
        FontAwesomeSolid = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 17.f, &FontAwesomeConfig, FontAwesomeRanges);
        FontAwesomeSolid14 = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Solid_compressed_data, FontAwesome6Solid_compressed_size, 15.f, &FontAwesomeConfig, FontAwesomeRanges);
        FontAwesomeBrands = io.Fonts->AddFontFromMemoryCompressedTTF(FontAwesome6Brands_compressed_data, FontAwesome6Brands_compressed_size, 17.f, &FontAwesomeConfig, FontAwesomeRangesBrands);
#endif

        D3DX11CreateShaderResourceViewFromMemory(Device, rawData, sizeof(rawData), NULL, NULL, &Logo, NULL);
    }
}
