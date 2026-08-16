#include "../include/slate.h"
#include "../../Definations/Brand.hpp"

#if defined(PRODUCT_VARIANT_C) && PRODUCT_VARIANT_C
#define VARIANT_SCHEME_COLOR pcolor{ 99, 102, 241, 1.00f }
#define VARIANT_BG_TINT pcolor{ 18, 18, 18, 1.00f }
#define VARIANT_SIDEBAR_BG pcolor{ 20, 20, 20, 1.00f }
#define VARIANT_PANEL_BG pcolor{ 26, 26, 26, 1.00f }
#elif defined(PRODUCT_VARIANT_B) && PRODUCT_VARIANT_B
#define VARIANT_SCHEME_COLOR pcolor{ 140, 82, 255, 1.00f }
#define VARIANT_BG_TINT pcolor{ 12, 10, 20, 1.00f }
#define VARIANT_SIDEBAR_BG pcolor{ 18, 14, 30, 1.00f }
#define VARIANT_PANEL_BG pcolor{ 22, 18, 36, 1.00f }
#else
#define VARIANT_SCHEME_COLOR pcolor{ 217, 217, 217, 1.00f }
#define VARIANT_BG_TINT pcolor{ 0, 0, 0, 1.00f }
#define VARIANT_SIDEBAR_BG pcolor{ 14, 14, 14, 1.00f }
#define VARIANT_PANEL_BG pcolor{ 18, 18, 18, 1.00f }
#endif

void c_style::setup( ) {
	static bool init = false;
	if ( !init )
	{
		themes.resize( 2 );

		for ( auto& t : themes ) {
			t.colors.resize( pcol_count );
		}

		colors.resize( pcol_count );

		themes[0].colors[pcol_scheme] = pcolor{ 173, 104, 219, 1.0 };
		themes[0].colors[pcol_text] = pcolor{ 222, 222, 222 };
		themes[0].colors[pcol_text2] = pcolor{ 77, 77, 77 };
		themes[0].colors[pcol_text3] = pcolor{ 77, 77, 77, 0.6f };
		themes[0].colors[pcol_bg] = pcolor{ 6, 6, 6 };
		themes[0].colors[pcol_bg2] = pcolor{ 11, 11, 11, 0.8 };
		themes[0].colors[pcol_bg3] = pcolor{ 18, 18, 18 };
		themes[0].colors[pcol_bg4] = pcolor{ 22, 22, 22 };
		themes[0].colors[pcol_bg5] = pcolor{ 28, 28, 28 };
		themes[0].colors[pcol_bg6] = pcolor{ 188, 188, 188, 0.05f };
		themes[0].colors[pcol_bg7] = pcolor{ 9, 11, 12 };
		themes[0].colors[pcol_border] = pcolor{ 182, 192, 242, 0.05f };
		themes[0].colors[pcol_separator] = pcolor{ 255, 255, 255, 0.04f };
		themes[0].colors[pcol_checkboxdot] = pcolor{ 38, 38, 40 };
		themes[0].colors[pcol_textonscheme] = pcolor{ 0, 0, 0 };
		themes[0].colors[pcol_listbox] = pcolor{ 16, 16, 16, 0.5f };
		themes[0].colors[pcol_tab] = pcolor{ 42, 35, 39 };
		themes[0].colors[pcol_backdrop] = pcolor{ 255, 255, 255, 0.05f };

        themes[0].colors[pcol_bg] = VARIANT_BG_TINT;
        themes[0].colors[pcol_bg2] = pcolor{ 255, 255, 255, 0.03f };
        themes[0].colors[pcol_bg3] = VARIANT_PANEL_BG;
        themes[0].colors[pcol_bg4] = pcolor{ 38, 38, 42, 1.00f };
        themes[0].colors[pcol_bg5] = VARIANT_SIDEBAR_BG;
        themes[0].colors[pcol_bg6] = pcolor{ 48, 48, 52, 1.00f };
        themes[0].colors[pcol_bg7] = pcolor{ 32, 32, 36, 1.00f };
        themes[0].colors[pcol_border] = pcolor{ 255, 255, 255, 0.06f };
        themes[0].colors[pcol_scheme] = VARIANT_SCHEME_COLOR;
        themes[0].colors[pcol_text] = pcolor{ 245, 245, 247, 1.00f };
        themes[0].colors[pcol_text2] = pcolor{ 156, 163, 175, 1.00f };
        themes[0].colors[pcol_text3] = pcolor{ 107, 114, 128, 1.00f };
        themes[0].colors[pcol_separator] = pcolor{ 255, 255, 255, 0.05f };


		themes[1].colors[pcol_scheme] = pcolor{ 173, 104, 219, 1.0 };
		themes[1].colors[pcol_text] = pcolor{ 11, 11, 11 };
		themes[1].colors[pcol_text2] = pcolor{ 122, 122, 122 };
		themes[1].colors[pcol_text3] = pcolor{ 122, 122, 122, 0.6f };
		themes[1].colors[pcol_bg] = pcolor{ 255, 255, 255, 0.9 };
		themes[1].colors[pcol_bg2] = pcolor{ 245, 245, 245, 0.8 };
		themes[1].colors[pcol_bg3] = pcolor{ 255, 255, 255 };
		themes[1].colors[pcol_bg4] = pcolor{ 252, 252, 252 };
		themes[1].colors[pcol_bg5] = pcolor{ 222, 222, 222, 0.9f };
		themes[1].colors[pcol_bg6] = pcolor{ 188, 188, 188, 0.25f };
		themes[1].colors[pcol_bg7] = pcolor{ 255, 255, 255 };
		themes[1].colors[pcol_border] = pcolor{ 0, 0, 0, 0.3f };
		themes[1].colors[pcol_separator] = pcolor{ 255, 255, 255, 0.04f };
		themes[1].colors[pcol_checkboxdot] = pcolor{ 38, 38, 40 };
		themes[1].colors[pcol_textonscheme] = pcolor{ 0, 0, 0 };
		themes[1].colors[pcol_listbox] = pcolor{ 16, 16, 16, 0.5f };
		themes[1].colors[pcol_tab] = pcolor{ 42, 35, 39 };
		themes[1].colors[pcol_backdrop] = pcolor{ 0, 0, 0, 0.15f };

		colors = themes[theme].colors;
		init = true;
	}

	for ( int i = 0; i < colors.size( ); ++i ) {
		colors[i].r = ImLerp( colors[i].r, themes[theme].colors[i].r, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].g = ImLerp( colors[i].g, themes[theme].colors[i].g, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].b = ImLerp( colors[i].b, themes[theme].colors[i].b, ImGui::GetIO( ).DeltaTime * 17 );
		colors[i].a = ImLerp( colors[i].a, themes[theme].colors[i].a, ImGui::GetIO( ).DeltaTime * 17 );
	}

	GImGui->Style.Colors[ImGuiCol_Text] = colors[pcol_text];
	GImGui->Style.Colors[ImGuiCol_TextDisabled] = colors[pcol_text2];
	GImGui->Style.Colors[ImGuiCol_WindowBg] = ImVec4{ 0, 0, 0, 0 }; // transparent — menu draws its own bg
	GImGui->Style.Colors[ImGuiCol_ChildBg] = colors[pcol_bg2];
	GImGui->Style.Colors[ImGuiCol_Border] = colors[pcol_border];
	GImGui->Style.Colors[ImGuiCol_TextSelectedBg] = colors[pcol_scheme].alpha( 0.3f );
	GImGui->Style.Colors[ImGuiCol_WindowShadow] = pcolor{ 0.f, 0.f, 0.f, 0.0f };
	GImGui->Style.WindowShadowSize = 0.f;

	auto& style = GImGui->Style;

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
	style.WindowRounding = 12.f;
	style.FramePadding = vec2{ 10, 6 };
	style.ItemSpacing = vec2{ 10, 8 };
	style.FrameRounding = 8.f;
	style.PopupRounding = 10.f;
	style.ChildRounding = 12.f;
#else
	style.WindowRounding = 8 ds;
	style.FramePadding = vec2{ 12, 10 };
	style.ItemSpacing = vec2{ 14, 14 };
	style.FrameRounding = 2 ds;
	style.PopupRounding = 2 ds;
	style.ChildRounding = 4 ds;
#endif
    style.WindowPadding = ImVec2{ 0, 0 };
    style.WindowBorderSize = 0;
    style.FrameBorderSize = 0;
    style.PopupBorderSize = 0;
    style.ChildBorderSize = 1;

    style.ItemInnerSpacing = vec2{ 10, 4 };

    style.ScrollbarRounding = 4;
    style.ScrollbarSize = 4;
    style.WindowMinSize = ImVec2{ 1, 1 };
}



void c_style::push( pcol_ col, pcolor newcol )
{
	pushcache.push_back( { col, g_style->col( col ) } );
	colors[col] = newcol;
}
void c_style::pop( int k )
{
	for ( int i = 0; i < k; ++i ) {
		auto& col = pushcache[pushcache.size( ) - 1];
		colors[col.col] = col.oldcol;
		pushcache.pop_back( );
	}
}