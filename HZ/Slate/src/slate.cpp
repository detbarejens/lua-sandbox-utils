#include "../include/slate.h"
#include "../resources/fonts.h"
#include "../resources/images.h"
#include "../../Definations/Brand.hpp"
#if defined(BRAND_TRINITY_LOGO) && BRAND_TRINITY_LOGO
#include "../resources/trinity_logo.h"
#endif

using namespace ImGui;

void c_slate::initialize_fonts( ) {
	fonts.clear( );
	fonts.resize( fonts_size );
	const static ImWchar icons_ranges[] = { 0xE900, 0xF5FD, 0 };

    fonts.at( ttsupermolotneuetrl_md ).setup( ttsupermolotneuetrl_mdbinary, sizeof( ttsupermolotneuetrl_mdbinary ),
		{ 16, 14 },
		GetIO( ).Fonts->GetGlyphRangesCyrillic( ) );

	fonts.at( ttsupermolotneuetrl_db ).setup( ttsupermolotneuetrl_dbbinary, sizeof( ttsupermolotneuetrl_dbbinary ),
		{ 16, 12, 14 },
		GetIO( ).Fonts->GetGlyphRangesCyrillic( ) );

	fonts.at( icons ).setup( iconsbinary, sizeof( iconsbinary ),
		{ 16, 12, 14, 18, 17 },
		icons_ranges );
}

bool bools[100];
int ints[100];
float floats[100];
bool multicombov[6];
bool multicombov2[6];
float col[100][4];

c_image logo;
void c_slate::initialize( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain ) {
	GImGui->IO.IniFilename = "";
	g_style->dpi_scale = 1;
	g_style->setup( );
	g_lang->initialize( );

	initialize_fonts( );

#if defined(BRAND_TRINITY_LOGO) && BRAND_TRINITY_LOGO
	logo.load( device, trinity_logobinary, trinity_logobinary_size );
#else
	logo.load( device, logobinary, sizeof( logobinary ) );
#endif

	widgets->nav.tabs = {
        { aiming_2_filled, "Aimbot", { { settings_3_filled, "Main" }, { sword_filled, "Weapon" }, { more_3_filled, "Other" } } },
        { eye_2_filled, "Visuals", {  } },
        { world_2_filled, "World", {  } },
        { group_3_filled, "Player List", {  } },
        { car_2_filled, "Vehicle List", {  } },
        { more_3_filled, "Misc", {  } },
        { folder_filled, "Configs", {  } },
    };

    widgets->nav.addpage( 0, [&] {
		static pcolor ammo_col = pcolor{ 0, 117, 255 };
		static pcolor healthbar_col = pcolor{ 131, 255, 131 };
		static pcolor name_col = pcolor{ 222, 222, 222 };
		static pcolor health_col = pcolor{ 100, 113, 143 };
		static pcolor armor_col = pcolor{ 100, 113, 143 };
		static pcolor admin_col = pcolor{ 100, 113, 143 };
		static pcolor weapon_col = pcolor{ 100, 113, 143 };
		static pcolor box_col = pcolor{ 255, 255, 255 };
		static pcolor skeleton_col = pcolor{ 255, 255, 255 };

        BeginGroup( );
        {
            widgets->child.begin( "CHECKBOXES 2", 1 );
            {
                widgets->checkbox( "Bounding Box", &bools[0] );
                widgets->checkbox( "Health Bar", &bools[1] );
                widgets->checkbox( "Ammo Bar", &bools[3] );
                widgets->checkbox( "Name", &bools[5] );
                widgets->checkbox( "Health", &bools[6] );
                widgets->checkbox( "Armor", &bools[7] );
                widgets->checkbox( "Admin", &bools[8] );
                widgets->checkbox( "Weapon", &bools[9] );
            }
            widgets->child.end( );
        }
        EndGroup( );
        SameLine( );
        SetCursorPosY( GetCurrentWindow( )->Scroll.y + GImGui->Style.WindowPadding.y );
		widgets->child.begin( "ESP PREVIEW", 2, { 0, GetWindowHeight( ) - GImGui->Style.WindowPadding.y * 2 } );
		{
			static bool init = false;
			if ( !init )
			{
				g_esppreview->create_bar( "Healthbar", esppos_left, &bools[1], &healthbar_col.r, &bools[2] );
				g_esppreview->create_bar( "Ammobar", esppos_bottom, &bools[3], &ammo_col.r, &bools[4] );
				g_esppreview->create_text( "Name", "Michael Conors", esppos_top, &bools[5], &name_col.r );
				g_esppreview->create_text( "Health", "100HP", esppos_left, &bools[6], &health_col.r );
				g_esppreview->create_text( "Armor", "100", esppos_left, &bools[7], &armor_col.r );
				g_esppreview->create_text( "Admin", "ADMIN", esppos_left, &bools[8], &admin_col.r );
				g_esppreview->create_text( "Weapon", "AK-47", esppos_bottom, &bools[9], &weapon_col.r );
				init = true;
			}

			g_esppreview->draw( GetCurrentWindow( )->Rect( ).GetCenter( ), &bools[0], &box_col.r, &ints[99], &bools[10], &skeleton_col.r, 0, &ints[15] );
		}
		widgets->child.end( );
    } );
    
}

void c_slate::draw( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain ) {
	m_device = device;
	m_ctx = ctx;
	m_swapchain = swapchain;

	g_style->setup( );
	g_shadertoy->setup( device, ctx, swapchain );

	SetNextWindowPos( { 555, 555 }, ImGuiCond_Once );
	
	widgets->window.begin( "menu", vec2{ 744, 464 }, ImGuiWindowFlags_NoBringToFrontOnFocus );
    {
        g_shadertoy->bg( GetWindowDrawList( ), GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), pcolor{ 255, 255, 255, 0.06f }, GImGui->Style.WindowRounding );
        BeginChild( "sidebar", vec2{ 74 + ( 166 - 74 ) * sidebar_anim, 0 }, 0, 128 );
        {
            GetWindowDrawList( )->AddRectFilled( GetWindowPos( ) + ImVec2{ GetWindowWidth( ) - 1, 0 }, GetWindowPos( ) + GetWindowSize( ), g_style->col( pcol_border ) );

            static c_buffer* buffer = new c_buffer( );
            buffer->glow_pos = GetWindowPos( ) + ImVec2{ GetWindowWidth( ) / 2, 49 ds };
            buffer->glow_strength = 48 ds;
            
            g_shadertoy->draw( GetWindowDrawList( ), GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), g_style->col( pcol_scheme, 0.07f ), GImGui->Style.WindowRounding, buffer );

            SetCursorPos( { GetWindowWidth( ) / 2 - 16 ds, 21.f ds } );
            Image( logo, vec2{ 32.f, 32.f } );
            widgets->nav.drawtabs( );

            sidebar_anim = anim( sidebar_anim, 0.f, 1.f, IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) );
        }
        EndChild( );
        SameLine( 0, 0.f ds );
        BeginChild( "main", vec2{ 0, 0 }, 0, 128 );
        {
            {
                static c_buffer* buffer = new c_buffer();
                buffer->glow_pos = GetWindowPos() + ImVec2{ 571 ds, 110 ds };
                buffer->glow_strength = 354 ds;

                g_shadertoy->draw(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_scheme, 0.14f), GImGui->Style.WindowRounding, buffer, ImDrawFlags_RoundCornersRight );
            }

            {
                static c_buffer* buffer = new c_buffer();
                buffer->glow_pos = GetWindowPos() + ImVec2{ 138 ds, 515 ds };
                buffer->glow_strength = 210 ds;

                g_shadertoy->draw(GetWindowDrawList(), GetWindowPos(), GetWindowPos() + GetWindowSize(), g_style->col(pcol_scheme, 0.14f), GImGui->Style.WindowRounding, buffer, ImDrawFlags_RoundCornersRight );
            }

            widgets->nav.drawsubtabs( );

            PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * widgets->nav.tab_anim * widgets->nav.subtab_anim );
            PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 20, 20 } );
            PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 10, 10 } );
            BeginChild( "page", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
            {
                widgets->child.smoothscroll( );
                if ( strlen( g_search->buf ) == 0 )
                    widgets->nav.drawpage( );
                else
                    g_search->draw( );
            
                SetCursorPos( GetCurrentWindow( )->Scroll );
                BeginChild( "gradient", { 100, 100 }, 0, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs );
                {
                    static float anim1 = 0; anim1 = anim( anim1, 0.f, 1.f, GetCurrentWindow( )->ParentWindow->Scroll.y > 0 );
                    static float anim2 = 0; anim2 = anim( anim2, 0.f, 1.f, GetCurrentWindow( )->ParentWindow->ScrollMax.y > 0 && GetCurrentWindow( )->ParentWindow->Scroll.y < ( GetCurrentWindow( )->ParentWindow->ScrollMax.y - 10 ) );
                
                    GetWindowDrawList( )->PushClipRect( GetCurrentWindow( )->ParentWindow->Rect( ).Min, GetCurrentWindow( )->ParentWindow->Rect( ).Max );
                    g_draw->gradient( GetWindowDrawList( ), GetCurrentWindow( )->ParentWindow->Pos, { GetCurrentWindow( )->ParentWindow->Rect( ).Max.x, GetCurrentWindow( )->ParentWindow->Pos.y + 60 ds }, g_style->col( pcol_bg, anim1 ), g_style->col( pcol_bg, anim1 ), g_style->col( pcol_bg, 0 ), g_style->col( pcol_bg, 0 ), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersNone );
                    g_draw->gradient( GetWindowDrawList( ), { GetCurrentWindow( )->ParentWindow->Pos.x, GetCurrentWindow( )->ParentWindow->Rect( ).Max.y - 30 ds }, GetCurrentWindow( )->ParentWindow->Rect( ).Max, g_style->col( pcol_bg, 0 ), g_style->col( pcol_bg, 0 ), g_style->col( pcol_bg, anim2 ), g_style->col( pcol_bg, anim2 ), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersBottomRight );
                    GetWindowDrawList( )->PopClipRect( );
                }
                EndChild( );
            }
            EndChild( );
            PopStyleVar( 3 );
        }
        EndChild( );
    }
    widgets->window.end( );


	widgets->notify.handle( );

	for ( auto& hotkey : widgets->keybinds.hotkeys ) {
		if ( hotkey.second.binds.empty( ) ) {
			continue;
		}

		auto& keybind = hotkey.second.binds[hotkey.second.selected];

		if ( keybind.mode == 1 ) {
			if ( GetAsyncKeyState( keybind.key ) & 1 ) {
				if ( hotkey.second.type == ht_checkbox ) {
					if ( !hotkey.second.is_active ) {
						hotkey.second.saved = *( bool* )hotkey.second.v;
						*( bool* )hotkey.second.v = *std::any_cast< bool >( &hotkey.second.value );
						hotkey.second.is_active = true;
					} else {
						*( bool* )hotkey.second.v = *std::any_cast< bool >( &hotkey.second.saved );
						hotkey.second.is_active = false;
					}
				} else if ( hotkey.second.type == ht_sliderint ) {
					if ( !hotkey.second.is_active ) {
						hotkey.second.saved = *( int* )hotkey.second.v;
						*( int* )hotkey.second.v = *std::any_cast< int >( &hotkey.second.value );
						hotkey.second.is_active = true;
					} else {
						*( int* )hotkey.second.v = *std::any_cast< int >( &hotkey.second.saved );
						hotkey.second.is_active = false;
					}
				} else if ( hotkey.second.type == ht_sliderfloat ) {
					if ( !hotkey.second.is_active ) {
						hotkey.second.saved = *( float* )hotkey.second.v;
						*( float* )hotkey.second.v = *std::any_cast< float >( &hotkey.second.value );
						hotkey.second.is_active = true;
					} else {
						*( float* )hotkey.second.v = *std::any_cast< float >( &hotkey.second.saved );
						hotkey.second.is_active = false;
					}
				}
			}
		} else {
			if ( hotkey.second.type == ht_checkbox ) {
				if ( GetAsyncKeyState( keybind.key ) & 1 && !widgets->keybinds.keyshandle[keybind.key] ) {
					hotkey.second.saved = *( bool* )hotkey.second.v;
					*( bool* )hotkey.second.v = *std::any_cast< bool >( &hotkey.second.value );
					hotkey.second.is_active = true;
				}
				if ( widgets->keybinds.keyshandle[keybind.key] && !GetAsyncKeyState( keybind.key ) && std::any_cast< bool >( &hotkey.second.saved ) ) {
					*( bool* )hotkey.second.v = *std::any_cast< bool >( &hotkey.second.saved );
					hotkey.second.is_active = false;
				}
			} else if ( hotkey.second.type == ht_sliderint ) {
				if ( GetAsyncKeyState( keybind.key ) & 1 && !widgets->keybinds.keyshandle[keybind.key] ) {
					hotkey.second.saved = *( int* )hotkey.second.v;
					*( int* )hotkey.second.v = *std::any_cast< int >( &hotkey.second.value );
					hotkey.second.is_active = true;
				}
				if ( widgets->keybinds.keyshandle[keybind.key] && !GetAsyncKeyState( keybind.key ) && std::any_cast< int >( &hotkey.second.saved ) ) {
					*( int* )hotkey.second.v = *std::any_cast< int >( &hotkey.second.saved );
					hotkey.second.is_active = false;
				}
			} else if ( hotkey.second.type == ht_sliderfloat ) {
				if ( GetAsyncKeyState( keybind.key ) & 1 && !widgets->keybinds.keyshandle[keybind.key] ) {
					hotkey.second.saved = *( float* )hotkey.second.v;
					*( float* )hotkey.second.v = *std::any_cast< float >( &hotkey.second.value );
					hotkey.second.is_active = true;
				}
				if ( widgets->keybinds.keyshandle[keybind.key] && !GetAsyncKeyState( keybind.key ) && std::any_cast< float >( &hotkey.second.saved ) ) {
					*( float* )hotkey.second.v = *std::any_cast< float >( &hotkey.second.saved );
					hotkey.second.is_active = false;
				}
			}
		}
	}

	for ( int i = 0; i < 166; ++i ) {
		widgets->keybinds.keyshandle[i] = GetAsyncKeyState( i );
	}
}