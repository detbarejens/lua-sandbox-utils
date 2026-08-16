#define NOMINMAX
#include "../include/slate.h"
#include "../../Definations/Brand.hpp"
#ifdef IMGUI_ENABLE_FREETYPE
#include "../../ImGui/imgui_freetype.h"
#endif
#include <thread>
#include <cctype>
#pragma comment(lib, "winmm.lib")

using namespace ImGui;

vec2::operator ImVec2( ) const {
	return ImVec2{ x, y } ds;
}

extern c_image logo;
void c_font::setup( unsigned char* data, size_t data_size, std::vector< float > sizes, const ImWchar* ranges ) {
	fonts.clear( );
	auto config = ImFontConfig( );
	config.FontDataOwnedByAtlas = false;

	for ( auto& sz : sizes ) {
		fonts.push_back( { sz, ImGui::GetIO( ).Fonts->AddFontFromMemoryTTF( data, data_size, sz * g_style->dpi_scale, &config, ranges ) } );
	}
}

bool c_widgets::button( const ptext& label, ImVec2 size, c_buttonstyle style ) 
{
	struct s {
		float hover;
		float held;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );

	auto* window = GetCurrentWindow( );
	bool pressed = InvisibleButton( label.str.data( ), CalcItemSize( size, GImGui->Style.FramePadding.x * 2 + CalcTextSize( label.translate( ).data( ), 0, 1 ).x + ( ( style.iconsize + 8 ) ds ) * bool( style.icon ), GetFrameHeight( ) ) );
	bool hovered = IsItemHovered( ), held = IsItemActive( );
	ImRect& bb = GImGui->LastItemData.Rect;

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );

	pcolor iconcolor, labelcolor;
	switch ( style.style ) {
	case button_outline:
	{
		iconcolor = g_style->col( pcol_scheme );
		labelcolor = col_anim( g_style->col( pcol_scheme ), g_style->col( pcol_text ), obj.hover );
		const float rounding = style.rounding > 0.f ? style.rounding : 8.f;
		window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( g_style->col( pcol_scheme, 0.00f ), g_style->col( pcol_scheme, 0.12f ), obj.hover ), rounding );
		window->DrawList->AddRect( bb.Min, bb.Max, col_anim( g_style->col( pcol_scheme, 0.70f ), g_style->col( pcol_scheme ), obj.hover ), rounding, 0, 1.4f );
	}
	break;
	case button_default:
	default:
	{
		iconcolor = g_style->col( pcol_scheme );
		labelcolor = g_style->col( pcol_text );
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		const float rounding = style.rounding > 3.f ? style.rounding : 8.f;
#else
		const float rounding = style.rounding;
#endif
		window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.hover ), rounding );
	}
	break;
	}

	float spacing = style.iconsize + 8;
    ImVec2 label_pos = bb.GetCenter( ) - CalcTextSize( label.translate( ).data( ), 0, 1 ) / 2;
    if ( style.icon )
    {
        label_pos.x = style.iconpos == align_left ? label_pos.x + ( spacing / 2 ) ds : label_pos.x - ( spacing / 2 ) ds;
        if ( CalcTextSize( label.translate( ).data( ), 0, 1 ).x == 0 ) label_pos.x = bb.GetCenter( ).x + 8 ds + ( style.iconsize ds ) / 2;
        float iconpos = style.iconpos == align_left ? label_pos.x - spacing ds : label_pos.x + CalcTextSize( label.translate( ).data( ), 0, 1 ).x + 8 ds;
        g_draw->text( icons, style.iconsize, { iconpos, bb.GetCenter( ).y - ( style.iconsize ds ) / 2 }, iconcolor, style.icon );
    }
    
    window->DrawList->AddText( label_pos, labelcolor, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	return pressed;
}
bool c_widgets::checkbox( const ptext& label, bool* v, c_checkboxstyle style ) 
{
    g_search->add_item( { label.str, [=] { checkbox( label, v, style ); } } );

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    if ( style.key == nullptr && style.col.empty( ) && !style.options )
        return trinity_toggle_row( label, v );

    {
        const float rowW = CalcItemWidth( );
        const float rowH = 28.f;
        const float pillW = 36.f;
        const float extrasW = ( style.key ? 64.f : 0.f ) + ( float )style.col.size( ) * 24.f + ( style.options ? 26.f : 0.f );
        const float rightW = extrasW + pillW + 10.f;
        const ImVec2 rowStart = GetCursorScreenPos( );
        const ImRect rowBb{ rowStart, rowStart + ImVec2{ rowW, rowH } };

        SetCursorScreenPos( rowStart );
        const bool pressed = InvisibleButton( label.str.data( ), ImVec2{ ImMax( 24.f, rowW - rightW - 8.f ), rowH } );
        if ( pressed )
            *v = !*v;

        const char* labelText = label.translate( ).data( );
        const ImVec2 labelSize = CalcTextSize( labelText, 0, true );
        GetWindowDrawList( )->AddText(
            ImVec2{ rowBb.Min.x, rowBb.Min.y + ( rowH - labelSize.y ) * 0.5f },
            *v ? g_style->col( pcol_text ) : g_style->col( pcol_text2 ),
            labelText, FindRenderedTextEnd( labelText ) );

        float cursorX = rowBb.Max.x - rightW;
        SetCursorScreenPos( ImVec2{ cursorX, rowBb.Min.y + ( rowH - 22.f ) * 0.5f } );
        BeginGroup( );
        char temp[128];
        if ( style.key ) {
            ImFormatString( temp, sizeof( temp ), "##%s key", label.str.data( ) );
            widgets->binder( temp, style.key );
            SameLine( 0, 6.f );
        }
        if ( !style.col.empty( ) ) {
            for ( int i = 0; i < ( int )style.col.size( ); ++i ) {
                ImFormatString( temp, sizeof( temp ), "##%s col %d", label.str.data( ), i );
                SetCursorPosY( GetCursorPosY( ) + 3.f );
                widgets->colorbutton( temp, style.col[i] );
                SameLine( 0, 6.f );
            }
        }
        if ( style.options ) {
            ImFormatString( temp, sizeof( temp ), "##%s settings", label.str.data( ) );
            widgets->optionsbtn( temp, style.options );
            SameLine( 0, 6.f );
        }
        EndGroup( );

        char toggleId[128];
        ImFormatString( toggleId, sizeof( toggleId ), "%s##%p", label.str.data( ), static_cast< void* >( v ) );
        SetCursorScreenPos( ImVec2{ rowBb.Max.x - pillW, rowBb.Min.y + ( rowH - 20.f ) * 0.5f } );
        widgets->trinity_toggle( toggleId, v );

        SetCursorScreenPos( rowBb.Min );
        Dummy( ImVec2{ rowW, rowH } );
        return pressed;
    }
#endif

    struct s {
        float anim;
        float hover;
        float enabled;
        ImVec2 optsize;
    }; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    
    float square_sz = 16 ds;
    
    bool pressed = InvisibleButton( label.str.data( ), { CalcItemWidth( ), square_sz } );
    bool hovered = IsItemHovered( ), held = IsItemActive( );
    ImRect total_bb = GImGui->LastItemData.Rect;
    ImRect bb{ { total_bb.Max.x - 16 ds, total_bb.Max.y - 16 ds }, { total_bb.Max.x - 0 ds, total_bb.Max.y } };
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered && !*v );
    obj.enabled = anim( obj.enabled, 0.f, 1.f, *v );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || *v );
    
    if ( pressed ) {
        *v = !*v;
    }
    
    auto bgcol = col_anim( col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_scheme ), obj.enabled );
    auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.enabled );
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg2 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_scheme ), obj.enabled ), 2 ds );
    auto dotcol = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_bg2 ), obj.enabled );
    window->DrawList->AddText( total_bb.Min, col, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
    window->DrawList->AddRect( bb.Min - vec2{ 2, 2 } * obj.hover, bb.Max + vec2{ 2, 2 } * obj.hover, col_anim( col_anim( g_style->col( pcol_scheme, 0.f ), g_style->col( pcol_scheme, 0.12f ), obj.hover ), g_style->col( pcol_scheme, 0.f ), obj.enabled ), 4 ds, 240 );
    window->DrawList->AddCircleFilled( bb.Min + vec2{ 6.f, 6.f } + vec2{ 2.f, 2.f }, 2.f, col_anim( col_anim( g_style->col( pcol_bg5, 0.f ), g_style->col( pcol_scheme, 0.2f ), obj.hover ), g_style->col( pcol_bg5 ), obj.enabled ) );
    
    
    auto pos = window->DC.CursorPos;
    auto posprev = window->DC.CursorPosPrevLine;
    
    window->DC.CursorPos = ImVec2{ bb.Min.x - obj.optsize.x - 8 ds, bb.GetCenter( ).y - obj.optsize.y / 2 };
    char temp[128];
    ImFormatString( temp, sizeof( temp ), "%s opt", label.str.data( ) );
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 1, 1 } );
    BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground );
    {
        if ( style.key ) {
            ImFormatString( temp, sizeof( temp ), "##%s key", label.str.data( ) );
            widgets->binder( temp, style.key );
            SameLine( 0, 8 ds );
        }
        if ( style.col.size( ) > 0 ) {
            for ( int i = 0; i < style.col.size( ); ++i ) {
                ImFormatString( temp, sizeof( temp ), "##%s col %d", label.str.data( ), i );
                SetCursorPosY( GetWindowHeight( ) / 2 - 6 ds );
                widgets->colorbutton( temp, style.col[i] );
                SameLine( 0, 8 ds );
            }
        }
        if ( style.options ) {
            ImFormatString( temp, sizeof( temp ), "##%s settings", label.str.data( ) );
            SetCursorPosY( GetWindowHeight( ) / 2 - 8 ds );
            widgets->optionsbtn( temp, style.options );
            SameLine( 0, 8 ds );
        }
        
        obj.optsize = GetWindowSize( );
    }
    EndChild( );
    PopStyleVar( );
    
    window->DC.CursorPos = pos;
    window->DC.CursorPosPrevLine = posprev;
    
    return pressed;
}

template < typename T >
bool c_widgets::slider( const ptext& label, T* v, T min, T max, const char* format, c_sliderstyle style ) 
{
    struct s {
        float anim;
        float hover;
        float held;
        float val_anim;
        bool washeld = false;
		float w;
    }; auto& obj = anim_obj( label.str.data( ), 1120, s{ } );
    
    char max_buf[32];
    ImFormatString( max_buf, sizeof( max_buf ), format, max );
    
    auto* window = GetCurrentWindow( );
    auto id = window->GetID( label.str.data( ) );
    ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcItemWidth(), 30 ds } };
    ImRect bb{ { total_bb.Min.x, total_bb.Max.y - 6.f ds }, { total_bb.Max.x - obj.w - 34 ds, total_bb.Max.y } };
    ItemSize( total_bb );
    ItemAdd( total_bb, id );
    
    bool result = false;
    
    auto data = GImGui->LastItemData;
    
    bool hovered, held;
    bool pressed = ButtonBehavior( total_bb, id, &hovered, &held );
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.held = anim( obj.held, 0.f, 1.f, held );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || held );
    obj.val_anim = ImLerp( obj.val_anim, ( ImClamp( *v, min, max ) - min * 1.f ) / ( max - min ) * bb.GetWidth( ), GetIO( ).DeltaTime * 17 );

	char buf[32];
    ImFormatString( buf, sizeof( buf ), format, *v );
    
    if ( held ) {
        *v = ImClamp( T( min + ( GetIO( ).MousePos.x - bb.Min.x ) / bb.GetWidth( ) * ( max - min ) ), min, max );   
        if ( !obj.washeld ) {
            obj.washeld = true;
        }
    } else { 
		if ( obj.washeld ) {
			obj.washeld = false;
			result = true;
		}
		 obj.w = ImLerp( obj.w, g_draw->textsize( ttsupermolotneuetrl_db, 14, buf ).x, GetIO( ).DeltaTime * 17 );
    }
    
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    window->DrawList->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg4 ), 4.f );
    window->DrawList->AddRectFilled( bb.Min, { bb.Min.x + obj.val_anim, bb.Max.y }, g_style->col( pcol_scheme ), 4.f );
    window->DrawList->AddCircleFilled( ImVec2{ bb.Min.x + obj.val_anim, bb.GetCenter( ).y }, 6.f, IM_COL32( 255, 255, 255, 255 ) );
#else
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg2 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_bg4 ), obj.held ), 3 ds );
    window->DrawList->AddShadowRect( bb.Min, { bb.Min.x + obj.val_anim, bb.Max.y }, g_style->col( pcol_scheme, 0.28f ), 28 ds, { 0, 0 } );
	window->DrawList->AddRectFilled( bb.Min, { bb.Min.x + obj.val_anim, bb.Max.y }, g_style->col( pcol_scheme ), 3 ds );
    window->DrawList->AddCircleFilled( ImVec2{ bb.Min.x + obj.val_anim, bb.GetCenter( ).y }, 7.f, g_style->col( pcol_bg6 ) );
#endif
    window->DrawList->AddText( total_bb.Min, g_style->col( pcol_text ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	PushStyleVar( ImGuiStyleVar_FramePadding, vec2{ 10, 8 } );
	PushStyleVar( ImGuiStyleVar_FrameRounding, 3 ds );
	PushFont( fonts[ttsupermolotneuetrl_db].get( 14 ) );
    
    auto pos = window->DC.CursorPos;
    window->DC.CursorPos = ImVec2{ total_bb.Max.x - CalcTextSize( buf ).x - GImGui->Style.FramePadding.x * 2, total_bb.Min.y };
    char temp[128];
    ImFormatString( temp, sizeof( temp ), "##%s", label );
    BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground );
    PushStyleColor( ImGuiCol_Text, g_style->col( pcol_text2 ).vec4( ) );
    PushStyleColor( ImGuiCol_FrameBg, g_style->col( pcol_bg3 ).vec4( ) );
    if ( InputTextEx( temp, "", buf, sizeof( buf ), CalcTextSize( buf ) + GImGui->Style.FramePadding * 2, ImGuiInputTextFlags_NoHorizontalScroll ) ) {
        DataTypeApplyFromText( buf, std::is_same< T, int >::value ? ImGuiDataType_S32 : ImGuiDataType_Float, v, format );
        *v = ImClamp( *v, min, max );
    }
    PopStyleColor( 2 );
    EndChild( );
    window->DC.CursorPos = pos;

	PopFont( );
	PopStyleVar( 2 );
    
    GImGui->LastItemData = data;
    
    return result;
}

bool c_widgets::sliderint( const ptext& label, int* v, int min, int max, const char* format ) 
{
	bool result = slider( label, v, min, max, format );
	keybinds.popup( label.str, v, ht_sliderint, min, max );

	return result;
}
bool c_widgets::sliderfloat( const ptext& label, float* v, float min, float max, const char* format ) 
{
	bool result = slider( label, v, min, max, format );
	keybinds.popup( label.str, v, ht_sliderfloat, min, max );

	return result;
}

bool c_widgets::colorbutton( const std::string& str_id, float* col )
{
    struct s {
        float anim;
        float hover;
        float open_anim;
        bool open;
    }; auto& obj = anim_obj( str_id.c_str( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    bool pressed = InvisibleButton( str_id.c_str( ), vec2{ 16, 16 } );
    bool hovered = IsItemHovered( );
    ImRect bb = GImGui->LastItemData.Rect;
    bool value_changed = false;
    
    if ( pressed ) {
        obj.open = !obj.open;
    }
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.open );
    
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    window->DrawList->AddRectFilled( bb.Min, bb.Max, pcolor{ col[0], col[1], col[2], 1.00f }, 4.f );
    window->DrawList->AddRect( bb.Min, bb.Max, IM_COL32( 255, 255, 255, 28 ), 4.f );
#else
    window->DrawList->AddRectFilled( bb.Min, bb.Max, pcolor{ col[0], col[1], col[2], 0.40f }, 2 ds );
    window->DrawList->AddRect( bb.Min, bb.Max, pcolor{ col[0], col[1], col[2], 1.00f }, 2 ds );
#endif
    
    
    if ( popup.begin( str_id.c_str( ), obj.open_anim, bb.Min, vec2{ 14, 14 } ) ) {
        if ( ( !IsWindowHovered( ImGuiHoveredFlags_AnyWindow ) || ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) ) || FindWindowDisplayIndex( GImGui->HoveredWindow ) < FindWindowDisplayIndex( GetCurrentWindow( ) ) ) && IsMouseClicked( 0 ) ) {
            obj.open = false;
        }
        
        PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 6, 6 } );
        colorpicker.draw( str_id.c_str( ), col );
        PopStyleVar( );
        
        popup.end( );
    }
    
    return value_changed;
}

bool c_widgets::coloredit( const ptext& label, float* col )
{
	TextEx( label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
	SameLine( CalcItemWidth( ) - 16 ds + GImGui->Style.WindowPadding.x );
	return colorbutton( label.str.data( ), col );
}
bool c_widgets::optionsbtn( const std::string_view& str_id, std::function< void( ) > options, ImVec2 windowpos, vec2 padding )
{
    struct s {
        float anim;
        float hover;
        float open_anim;
        bool open = false;
        bool closed = false;
    }; auto& obj = anim_obj( str_id.data( ), 12210, s{ } );
    
    auto* window = GetCurrentWindow( );
    bool pressed = InvisibleButton( str_id.data( ), { 20, 20 } );
    bool hovered = IsItemHovered( );
    ImRect bb = GImGui->LastItemData.Rect;
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.open );
    
    if ( pressed && !obj.closed ) {
        obj.open = true;
    }
    
    if ( !IsMouseDown( 0 ) ) {
        obj.closed = false;
    }
    
    if ( windowpos.x == 0 && windowpos.y == 0 ) {
        windowpos = ImVec2{ bb.Min.x, bb.Max.y + 8 ds };
    }
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg2, 0.f ), g_style->col( pcol_bg2 ), obj.hover ), g_style->col( pcol_bg4 ), obj.open_anim ), 16 ds );
    g_draw->text( icons, 12, bb.Min + vec2{ -2496.f, 146.f }, col_anim( pcolor{ 89, 89, 89, 1.00f }, pcolor{ 219, 219, 219, 1.00f }, obj.open_anim ), settings_3_filled );
    
    
    if ( popup.begin( str_id, obj.open_anim, windowpos, padding ) ) {
        if ( ( !IsWindowHovered( ImGuiHoveredFlags_AnyWindow ) || ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) ) ) && IsMouseClicked( 0 ) ) {
            obj.open = false;
            obj.closed = true;
        }
        
        PushItemWidth( 180 ds );
        PushItemFlag( ImGuiItemFlags_NoNav, true );
        options( );
        PopItemFlag( );
        PopItemWidth( );
        popup.end( );
    }
    
    return pressed;
}
bool c_widgets::iconbutton( const char* str_id, const char* icon )
{
	struct s {
		float anim = 0;
		float hover = 0;
		float held = 0;
	}; auto& obj = anim_obj( str_id, 0, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id );
	ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };
	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || held );

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text3, 0.6f ), obj.held );

	g_draw->text( icons, 16, bb.Min, col, icon );

	return pressed;
}

bool c_widgets::comboex( const ptext& label, const std::string_view& preview, bool should_close, c_combostyle style ) 
{
    struct s {
        float anim;
        float hover;
        float held;
        float open_anim;
        bool open;
        float w;
    }; auto& obj = anim_obj( label.str.data( ), 123123443, s{ } );
    
    bool result = false;
    
    obj.w = ImLerp( obj.w, g_draw->textsize( ttsupermolotneuetrl_db, 16, preview.data( ) ).x, GetIO( ).DeltaTime * 17 );
    
    ImVec2 padding{ 10 ds, 8 ds };
    
    auto* window = GetCurrentWindow( );
    auto id = window->GetID( label.str.data( ) );
    ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcItemWidth( ), 50 ds } };
    ImRect bb{ { total_bb.Min.x, total_bb.Max.y - 32 ds }, { total_bb.Max.x, total_bb.Max.y } };
    
    ItemSize( total_bb );
    ItemAdd( total_bb, id );
    
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );
    
    if ( pressed ) {
        obj.open = !obj.open;
    }
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.held = anim( obj.held, 0.f, 1.f, held );
    obj.anim = anim( obj.anim, 0.f, 1.f, obj.open || hovered );
    obj.open_anim = anim( obj.open_anim, 0.f, 1.f, obj.open );
    
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg4 ), g_style->col( pcol_bg6 ), obj.hover ), g_style->col( pcol_bg6 ), obj.open_anim ), 8.f );
#else
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( col_anim( g_style->col( pcol_bg2 ), g_style->col( pcol_bg4 ), obj.hover ), g_style->col( pcol_bg4 ), obj.open_anim ), 3 ds );
#endif
    g_draw->text( ttsupermolotneuetrl_db, 12, total_bb.Min, g_style->col( pcol_text2 ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
    window->DrawList->AddText( bb.Min + padding, g_style->col( pcol_text ), preview.data( ) );
    g_draw->rotatestart( );
    g_draw->text( icons, 16, vec2{ bb.Max.x - 26.f, bb.Min.y + 8.f }, g_style->col( pcol_text2 ), down_small_filled );
    g_draw->rotateend( IM_PI / 2 - IM_PI * obj.open_anim, g_draw->rotationcenter( ) );
    
    
    if ( obj.open_anim > 0.05f ) {
        char temp[64];
        ImFormatString( temp, sizeof( temp ), "%s popup", label.str.data( ) );
        PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 0, 4 } );
        PushStyleVar( ImGuiStyleVar_WindowRounding, style.rounding );
        PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1 );
        PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 0, 0 } );
        PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * obj.open_anim );
        PushStyleColor( ImGuiCol_WindowBg, g_style->col( pcol_bg7 ).vec4( ) );
        Begin( temp, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize );
        SetWindowSize( { ImMax( GetCurrentWindow( )->ContentSize.x, bb.GetWidth( ) ), ImMin( GetCurrentWindow( )->ContentSize.y + GImGui->Style.WindowPadding.y * 2 + 2 ds, 200.f ds ) * obj.open_anim } );
        SetWindowPos( { bb.Max.x - GetWindowWidth( ), bb.GetCenter( ).y - GetWindowHeight( ) / 2 } );
        BringWindowToDisplayFront( GetCurrentWindow( ) );
        BringWindowToFocusFront( GetCurrentWindow( ) );
        
        if ( GImGui->HoveredWindow && !strstr( GImGui->HoveredWindow->Name, "popup" ) && IsMouseClicked( 0 ) && !hovered || should_close ) {
            obj.open = false;
        }   
        
        return true;
    }
    
    return false;
}

bool c_widgets::combo( const ptext& label, int* v, std::vector< std::string_view > items ) 
{
	struct s {
		bool should_close;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
	float maxw = 0;
	for ( int i = 0; i < items.size( ); ++i ) {
		maxw = ImMax( CalcTextSize( items[i].data( ), 0, 1 ).x + 48 ds, maxw );
	}

	if ( comboex( label, items[*v], obj.should_close ) ) {
		if ( obj.should_close ) obj.should_close = false;

		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], *v == i, { ImMax( maxw, GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 ), 28 ds } ) ) {
				*v = i;
				obj.should_close = true;
			}
		}

		End( );
		PopStyleColor( );
		PopStyleVar( 5 );
	}

	return obj.should_close;
}
bool c_widgets::multicombo( const ptext& label, bool* v, std::vector< std::string_view > items ) 
{
	auto& style = GetStyle( );

    std::string buf;

    buf.clear( );
    for ( size_t i = 0; i < items.size( ); ++i ) {
        if ( v[i] ) {
			buf += ptext{ items[i] }.translate( );
            buf += ", ";
        }
    }

    if ( !buf.empty( ) ) {
        buf.resize( buf.size( ) - 2 );
    }

	float maxw = 0;
	for ( int i = 0; i < items.size( ); ++i ) {
		maxw = ImMax( CalcTextSize( items[i].data( ), 0, 1 ).x + 48 ds, maxw );
	}

    if ( CalcTextSize( buf.c_str( ) ).x > 100 ds ) {
        for ( int i = 0; i < buf.size( ) - 1; ++i ) {
            if ( CalcTextSize( buf.substr( 0, i + 1 ).c_str( ) ).x > 100 ds ) {
                buf.resize( i );
                if ( buf[buf.size( ) - 1] == ',' ) {
                    buf.resize( buf.size( ) - 1 );
                }
                buf.append( ".." );
            }
        }
    }

	if ( buf == "" ) { 
		buf = "None";
	}

	bool result = false;
	if ( comboex( label, buf.c_str( ) ) ) {
		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], v[i], { ImMax( maxw, GetWindowWidth( ) ), 28 ds } ) ) {
				v[i] = !v[i];
				result = true;
			}
		}

		End( );
		PopStyleColor( );
		PopStyleVar( 5 );
	}

	return result;
}
bool c_widgets::binder( const ptext& label, int* key )
{
	struct s {
		float anim;
		float hover;
		float active_anim;
		float w;
		bool active = false;
		bool wait_release = false;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
	const int keyIndex = (*key >= 0 && *key < (int)keys.size( )) ? *key : 0;
	std::string_view buf = obj.active ? "..." : keys[keyIndex];
	obj.w = ImLerp( obj.w, CalcTextSize( buf.data( ) ).x, GetIO( ).DeltaTime * 17 );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( label.str.data( ) );
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
	const float chipH = 22.f;
	const float chipW = ImMax( 52.f, 18.f + obj.w );
	ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ? ImVec2{ CalcItemWidth( ), chipH } : ImVec2{ chipW, chipH } ) };
	ImRect bb{ total_bb.Max - ImVec2{ chipW, chipH }, total_bb.Max };
#else
	ImRect total_bb{ window->DC.CursorPos, window->DC.CursorPos + ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ? ImVec2{ CalcItemWidth( ), 26 ds } : ImVec2{ 16 ds + obj.w, 26 ds } ) };
	ImRect bb{ total_bb.Max - ImVec2{ ( 16 ds + int( obj.w ) ), 26 ds }, total_bb.Max };
#endif
	ItemSize( total_bb );
	ItemAdd( total_bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );
	bool value_changed = false;

	if ( pressed ) {
		obj.active = true;
		obj.wait_release = true;
	}

	if ( obj.active ) {
		binder_capturing = true;

		if ( obj.wait_release ) {
			bool waiting = false;
			for ( int i = 0; i < 5; ++i ) {
				if ( GetIO( ).MouseDown[i] )
					waiting = true;
			}
			if ( !waiting )
				obj.wait_release = false;
		} else {
			for ( auto i = 0; i < 5; i++ ) {
				if ( GetIO( ).MouseDown[i] ) {
					switch ( i ) {
					case 0:
						*key = VK_LBUTTON;
						break;
					case 1:
						*key = VK_RBUTTON;
						break;
					case 2:
						*key = VK_MBUTTON;
						break;
					case 3:
						*key = VK_XBUTTON1;
						break;
					case 4:
						*key = VK_XBUTTON2;
					}
					value_changed = true;
					obj.active = false;
				}
			}

			if ( !value_changed ) {
				for ( auto i = VK_BACK; i <= VK_RMENU; i++ ) {
					if ( GetAsyncKeyState( i ) & 0x8000 ) {
						*key = i == VK_ESCAPE ? 0 : i;
						value_changed = true;
						obj.active = false;
					}
				}
			}
		}
	}

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.active_anim = anim( obj.active_anim, 0.f, 1.f, obj.active );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || obj.active );

	auto bgcol = col_anim( g_style->col( pcol_bg4 ), g_style->col( pcol_bg6 ), obj.anim );
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
	window->DrawList->AddRectFilled( bb.Min, bb.Max, bgcol, 6.f );
	if ( obj.active_anim > 0.02f )
		window->DrawList->AddRect( bb.Min, bb.Max, g_style->col( pcol_scheme, 0.55f + 0.45f * obj.active_anim ), 6.f );
	const ImVec2 keySize = CalcTextSize( buf.data( ) );
	window->DrawList->AddText(
		ImVec2{ bb.Min.x + ( bb.GetWidth( ) - keySize.x ) * 0.5f, bb.Min.y + ( bb.GetHeight( ) - keySize.y ) * 0.5f },
		obj.active ? g_style->col( pcol_scheme ) : g_style->col( pcol_text2 ),
		buf.data( ) );
#else
	window->DrawList->AddRectFilled( bb.Min, bb.Max, bgcol, 2 ds );
	g_draw->text( ttsupermolotneuetrl_md, 14, bb.Min + vec2{ 8, 6 }, g_style->col( pcol_text ), buf.data( ) );
#endif

	window->DrawList->AddText( { total_bb.Min.x, total_bb.GetCenter( ).y - GImGui->FontSize / 2 }, g_style->col( pcol_text ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	return value_changed;
}
bool c_widgets::selector( const ptext& label, int* v, const std::vector< std::string_view >& items )
{
	struct s {
		ImVec2 size;
	}; auto& obj = anim_obj( label.str.data( ), 0, s{ } );

	bool result = false;

	GetWindowDrawList( )->AddText( GetCurrentWindow( )->DC.CursorPos + ImVec2{ 0, obj.size.y / 2 - GImGui->FontSize / 2 }, g_style->col( pcol_text ), label.translate( ).data( ) );
	Dummy( CalcTextSize( label.translate( ).data( ) ) );
	SameLine( CalcItemWidth( ) - obj.size.x + GImGui->Style.WindowPadding.x );
	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 2, 2 } );
	PushStyleVar( ImGuiStyleVar_ChildRounding, 3 ds );
	BeginChild( label.str.data( ), { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
	{
		obj.size = GetWindowSize( );

		for ( int i = 0; i < items.size( ); ++i ) {
			if ( selectable( items[i], *v == i, { 0, CalcTextSize( ptext{ items[i] }.translate( ).data( ) ).y + 4 ds }, { .xpad = 4, .rounding = 2, .checkmark = false } ) ) {
				*v = i;
				result = true;
			}

			if ( i < items.size( ) - 1 )
				SameLine( 0, 2 ds );
		}
	}
	EndChild( );
	PopStyleVar( 2 );
	PopStyleColor( );

	return result;
}
bool c_widgets::selectable( const ptext& label, bool selected, ImVec2 size, c_selectablestyle style )
{
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( label.str.data( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    bool pressed = InvisibleButton( label.str.data( ), CalcItemSize( size, window->Size.y, GetFrameHeight( ) ) );
    bool hovered = IsItemHovered( );
    ImRect bb = GImGui->LastItemData.Rect;
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, col_anim( g_style->col( pcol_bg2, 0.f ), g_style->col( pcol_bg2 ), obj.selected ), 2 ds );
    g_draw->text( icons, 14, bb.Min + vec2{ 7.f, 28.f } + vec2{ 0.f, -21.f } * obj.selected, col_anim( g_style->col( pcol_text2, 0.f ), g_style->col( pcol_text2 ), obj.selected ), check_filled );
    g_draw->text( ttsupermolotneuetrl_md, 16, bb.Min + vec2{ 8.f, 6.f } + vec2{ 21.f, 0.f } * obj.selected, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.selected ), label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );
    
    
    return pressed;
}

void c_widgets::spacing( float px )
{
	SetCursorPosY( GetCursorPosY( ) - GImGui->Style.ItemSpacing.y + px );
}

void c_widgets::separator( bool vertical, float arg1, float arg2 )
{
	if ( vertical ) {
		if ( arg1 == -1 ) arg1 = GImGui->FontSize;
		if ( arg2 == -1 ) arg2 = GImGui->FontSize;

		GetWindowDrawList( )->AddRectFilled( GetCurrentWindow( )->DC.CursorPos + ImVec2{ 0, ( arg2 / 2 - arg1 / 2 ) ds }, GetCurrentWindow( )->DC.CursorPos + ImVec2{ 1, ( arg2 / 2 + arg1 / 2 ) ds }, g_style->col( pcol_separator ) );
		Dummy( { 1, arg1 } );
	} else {
		if ( arg1 == -1 ) arg1 = 14;
		if ( arg2 == -1 ) arg2 = 0;

		GetWindowDrawList( )->AddRectFilled( GetWindowPos( ) + ImVec2{ arg1 ds, GetCursorPosY( ) }, GetWindowPos( ) + ImVec2{ GetWindowWidth( ) - arg2 ds, GetCursorPosY( ) + 1 }, g_style->col( pcol_separator ) );
		Dummy( { GetWindowWidth( ) - arg1 - arg2, 1 } );
	}	
}

bool c_widgets::textinput( const ptext& label, char* buf, size_t buf_size, c_textinputstyle style )
{
	bool result = false;

	if ( !style.bgcol ) {
		style.bgcol = g_style->col( pcol_bg3 );
	}

	BeginGroup( );
	if ( CalcTextSize( label.str.data( ), 0, 1 ).x > 0 ) {
		Text( label.translate( ).data( ) );
		spacing( 8 ds );
	}

	PushStyleVar( ImGuiStyleVar_FramePadding, style.padding );
	ImVec2 size{ CalcItemSize( style.size, CalcItemWidth( ), GetFrameHeight( ) ) };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };
	GetWindowDrawList( )->AddRectFilled( bb.Min, bb.Max, style.bgcol, style.rounding );

	if ( GImGui->Style.FrameBorderSize > 0 ) {
		GetWindowDrawList( )->AddRect( bb.Min, bb.Max, g_style->col( pcol_border ), style.rounding );
	}

	PushStyleColor( ImGuiCol_FrameBg, GetColorU32( ImGuiCol_FrameBg, 0 ) );
	char temp[64];
	ImFormatString( temp, sizeof( temp ), "##%s", label.str.data( ) );

	if ( style.icon ) {
		g_draw->text( icons, 16, bb.Min + style.padding, g_style->col( pcol_text2 ), style.icon );
		SetCursorPosX( GetCursorPosX( ) + 26 ds );
	}

	PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0 );
	result = InputTextEx( temp, style.hint.data( ), buf, buf_size, size - ImVec2{ ( 26 ds ) * bool( style.icon ), 0 }, style.flags );
	PopStyleColor( );
	PopStyleVar( 2 );
	EndGroup( );

	return result;
}

///////////////////////////////////////////////////////////////////////////////


// WINDOW
void c_widgets::widget_window::begin( const std::string_view& name, ImVec2 size, ImGuiWindowFlags flags ) {
	SetNextWindowSize( size );
	Begin( name.data( ), 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | flags );
}
void c_widgets::widget_window::end( ) {
	End( );
}

// CHILD

void c_widgets::widget_child::begin( const ptext& name, int num, ImVec2 size ) {
	SetCursorPosY( GetCursorPosY( ) + 8 * ( 1.f - pow( GImGui->Style.Alpha, num ) ) );

    float padding = GImGui->Style.WindowPadding.x;
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
    PushStyleColor( ImGuiCol_Border, ImVec4( 1.f, 1.f, 1.f, 0.045f ) );
    PushStyleVar( ImGuiStyleVar_ChildRounding, 12.f );
    PushStyleVar( ImGuiStyleVar_ChildBorderSize, 1.f );
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 14, 14 } );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 10, 6 } );
    BeginChild( name.str.data( ), CalcItemSize( size, GetWindowWidth( ) / 2 - padding - GImGui->Style.ItemSpacing.x / 2, 0 ), ImGuiChildFlags_Border, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
    PushItemWidth( GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 );
    smoothscroll( false );
#else
    PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
    BeginChild( name.str.data( ), CalcItemSize( size, GetWindowWidth( ) / 2 - padding - GImGui->Style.ItemSpacing.x / 2, 0 ), 96, 0 );
    PopStyleVar( );

	g_draw->gradientoutline( GetWindowDrawList( ), GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), g_style->col( pcol_border ), g_style->col( pcol_border ), g_style->col( pcol_border, 0 ), g_style->col( pcol_border, 0 ), GImGui->Style.ChildRounding );
    
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 14, 12 } );
    BeginChild( "header", vec2{ 0, 28 }, 2, 128 | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
    {
        PopStyleVar( );
        PushStyleColor( ImGuiCol_Text, g_style->col( pcol_text2 ).vec4( ) );
        TextEx( name.translate( ).data( ), FindRenderedTextEnd( name.translate( ).data( ) ) );
        PopStyleColor( );
    }
    EndChild( );
    
    widgets->spacing( 0 ds );
    
    PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 14, 14 } );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 14, 14 } );
    PushStyleVar( ImGuiStyleVar_ChildRounding, 0 ds );
    BeginChild( "content", { 0, size.y == 0 ? 0 : size.y - GetCursorPosY( ) }, 98, 152 );
    PopStyleVar( );
    
    PushItemWidth( GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 );
    smoothscroll( false );
#endif
}


void c_widgets::widget_child::end( ) {
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
    PopItemWidth( );
    EndChild( );
    PopStyleVar( 4 );
    PopStyleColor( 2 );
#else
	PopItemWidth( );
	EndChild( );
	PopStyleVar( 2 );
	EndChild( );
#endif
}

/* Panel header strip: uppercase label with a rule underneath, matching the
   section headers used across the Trinity pages. */
static void trinity_header_strip( std::string_view label, float width, float rowH, bool* toggle ) {
    std::string upper( label );
    for ( char& c : upper )
        c = ( char )toupper( ( unsigned char )c );

    const ImVec2 pos = GetCursorScreenPos( );
    const ImVec2 textSize = CalcTextSize( upper.c_str( ) );

    ImVec2 textPos{ pos.x, pos.y + ( rowH - textSize.y ) * 0.5f };
    for ( const char* c = upper.c_str( ); *c; ++c ) {
        char buf[2] = { *c, 0 };
        GetWindowDrawList( )->AddText( textPos, IM_COL32( 156, 163, 175, 255 ), buf );
        textPos.x += CalcTextSize( buf ).x + 1.15f;
    }

    if ( toggle ) {
        char toggleId[128];
        ImFormatString( toggleId, sizeof( toggleId ), "%s##hdr%p", upper.c_str( ), ( void* )toggle );
        SetCursorScreenPos( ImVec2{ pos.x + width - 36.f, pos.y + ( rowH - 20.f ) * 0.5f } );
        widgets->trinity_toggle( toggleId, toggle );
    }

    GetWindowDrawList( )->AddLine(
        ImVec2{ pos.x, pos.y + rowH },
        ImVec2{ pos.x + width, pos.y + rowH },
        IM_COL32( 255, 255, 255, 14 ) );

    SetCursorScreenPos( pos );
    Dummy( ImVec2{ width, rowH } );
}

void c_widgets::trinity_section( const ptext& title ) {
    trinity_header_strip( title.translate( ), GetContentRegionAvail( ).x, 22.f, nullptr );
    widgets->spacing( 8 ds );
}

void c_widgets::trinity_section_header( const ptext& title, bool* toggle ) {
    trinity_header_strip( title.translate( ), GetContentRegionAvail( ).x, 28.f, toggle );
    widgets->spacing( 8 ds );
}

bool c_widgets::trinity_toggle( const char* id, bool* v ) {
    auto* window = GetCurrentWindow( );
    const ImVec2 size{ 36.f, 20.f };
    const ImVec2 pos = window->DC.CursorPos;

    struct s { float on; float hover; };
    auto& obj = anim_obj( id, 710, s{ } );

    PushID( id );
    bool pressed = InvisibleButton( "##toggle", size );
    PopID( );
    if ( pressed )
        *v = !*v;

    const bool hovered = IsItemHovered( );
    obj.on = anim( obj.on, 0.f, 1.f, *v );
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );

    const ImU32 accent = IM_COL32( BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255 );
    const ImU32 off = hovered ? IM_COL32( 58, 58, 64, 255 ) : IM_COL32( 46, 46, 52, 255 );
    const ImU32 track = col_anim( off, accent, obj.on );
    window->DrawList->AddRectFilled( pos, pos + size, track, 10.f );

    const float knob = 14.f;
    const float pad = 3.f;
    const float travel = size.x - knob - pad * 2.f;
    const float kx = pos.x + pad + travel * obj.on;
    const float ky = pos.y + ( size.y - knob ) * 0.5f;
    window->DrawList->AddCircleFilled( ImVec2{ kx + knob * 0.5f, ky + knob * 0.5f }, knob * 0.5f, IM_COL32( 255, 255, 255, 255 ) );
    return pressed;
}

bool c_widgets::trinity_toggle_row( const ptext& label, bool* v, float width ) {
    const float rowW = width > 0.f ? width : CalcItemWidth( );
    const float rowH = 28.f;
    const float toggleW = 36.f;
    const float gap = 10.f;

    const ImVec2 rowStart = GetCursorScreenPos();
    const ImRect rowBb{ rowStart, rowStart + ImVec2{ rowW, rowH } };

    char toggleId[128];
    ImFormatString( toggleId, sizeof( toggleId ), "%s##%p", label.str.data( ), static_cast<void*>( v ) );

    const char* labelText = label.translate( ).data( );
    const ImVec2 labelSize = CalcTextSize( labelText, 0, true );
    const float labelMaxW = ImMax( 20.f, rowW - toggleW - gap );
    const ImVec2 labelPos{
        rowBb.Min.x,
        rowBb.Min.y + ( rowH - labelSize.y ) * 0.5f
    };

    char rowId[140];
    ImFormatString( rowId, sizeof( rowId ), "%s##row", toggleId );
    SetCursorScreenPos( rowStart );
    bool rowPressed = false;
    if ( InvisibleButton( rowId, ImVec2{ labelMaxW, rowH } ) )
    {
        *v = !*v;
        rowPressed = true;
    }

    PushClipRect( rowBb.Min, rowBb.Min + ImVec2{ labelMaxW, rowH }, true );
    GetWindowDrawList( )->AddText( labelPos, *v ? g_style->col( pcol_text ) : g_style->col( pcol_text2 ), labelText, FindRenderedTextEnd( labelText ) );
    PopClipRect( );

    SetCursorScreenPos( ImVec2{ rowBb.Max.x - toggleW, rowBb.Min.y + ( rowH - 20.f ) * 0.5f } );
    const bool changed = trinity_toggle( toggleId, v );

    SetCursorScreenPos( rowBb.Min );
    Dummy( ImVec2{ rowW, rowH } );
    return changed || rowPressed;
}

void c_widgets::trinity_divider( ) {
    widgets->spacing( 6 ds );
    widgets->separator( false, 0, 0 );
    widgets->spacing( 6 ds );
}

void c_widgets::trinity_setting_row( const ptext& label, std::function< void( ) > trailing, float width ) {
    const float rowW = width > 0.f ? width : CalcItemWidth( );
    const float rowH = 28.f;
    const float trailingW = 112.f;
    const float gap = 10.f;

    const ImVec2 rowStart = GetCursorScreenPos();
    const ImRect rowBb{ rowStart, rowStart + ImVec2{ rowW, rowH } };

    const char* labelText = label.translate( ).data( );
    const ImVec2 labelSize = CalcTextSize( labelText, 0, true );
    const float labelMaxW = ImMax( 20.f, rowW - trailingW - gap );
    const ImVec2 labelPos{
        rowBb.Min.x,
        rowBb.Min.y + ( rowH - labelSize.y ) * 0.5f
    };

    PushClipRect( rowBb.Min, rowBb.Min + ImVec2{ labelMaxW, rowH }, true );
    GetWindowDrawList( )->AddText( labelPos, g_style->col( pcol_text2 ), labelText, FindRenderedTextEnd( labelText ) );
    PopClipRect( );

    SetCursorScreenPos( ImVec2{ rowBb.Max.x - trailingW, rowBb.Min.y } );
    BeginGroup( );
    trailing( );
    EndGroup( );

    SetCursorScreenPos( rowBb.Min );
    Dummy( ImVec2{ rowW, rowH } );
}

void c_widgets::trinity_key_chip( const char* label ) {
    auto* window = GetCurrentWindow( );
    const ImVec2 textSize = CalcTextSize( label, 0, true );
    const ImVec2 chipSize{ ImMax( 52.f, textSize.x + 16.f ), 22.f };
    const ImVec2 pos = window->DC.CursorPos;

    window->DrawList->AddRectFilled( pos, pos + chipSize, g_style->col( pcol_bg4 ), 6.f );
    window->DrawList->AddRect( pos, pos + chipSize, IM_COL32( 255, 255, 255, 16 ), 6.f );
    window->DrawList->AddText(
        pos + ImVec2{ ( chipSize.x - textSize.x ) * 0.5f, ( chipSize.y - textSize.y ) * 0.5f },
        g_style->col( pcol_text2 ),
        label,
        FindRenderedTextEnd( label )
    );

    Dummy( chipSize );
}

float* c_widgets::widget_child::smoothscroll( bool scrollbar, ImVec2 padding )
{
	struct s {
		ImVec2 scroll;
		ImVec2 scroll_anim;
		bool scroll_activey = false;
		bool scroll_activex = false;
		float oldscroll;
		float clickpos;

		float xanim;
		float yanim;
	}; auto& obj = anim_obj( GetCurrentWindow( )->Name, 23123, s{ } );

	obj.scroll.y = ImClamp( obj.scroll.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
	obj.scroll.x = ImClamp( obj.scroll.x, 0.f, GetCurrentWindow( )->ScrollMax.x );
	obj.scroll_anim.y = ImClamp( obj.scroll_anim.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
	obj.scroll_anim.x = ImClamp( obj.scroll_anim.x, 0.f, GetCurrentWindow( )->ScrollMax.x );

	if ( scrollbar ) {
		// vertical
		{
			ImRect scrollbarbb{ GetWindowPos( ) + ImVec2{ GetWindowWidth( ) - 5 ds, 4 ds + padding.y }, GetWindowPos( ) + GetWindowSize( ) - vec2{ 2 ds, 4 ds + padding.y } };
			float visiblepart = GetWindowHeight( ) / ( GetCurrentWindow( )->ContentSize.y + GImGui->Style.WindowPadding.y * 2 );

			if ( visiblepart < 1.f ) {
				float scrollh = scrollbarbb.GetHeight( ) * visiblepart;
				float invisiblepart = 1.f - visiblepart;
				float scrolloffset = ( obj.scroll.y / GetCurrentWindow( )->ScrollMax.y ) * ( scrollbarbb.GetHeight( ) - scrollh );

				obj.yanim = anim( obj.yanim, 0.f, 1.f, IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) || obj.scroll_activey );
				auto col = col_anim( g_style->col( pcol_scheme ), g_style->col( pcol_scheme, 0.8f ), obj.yanim );

				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + vec2{ 1, 0 }, scrollbarbb.Max - vec2{ 1, 0 }, g_style->col( pcol_bg3 ), 5 ds );
				GetWindowDrawList( )->PushClipRect( scrollbarbb.Min, scrollbarbb.Max );
				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + ImVec2{ 0, scrolloffset }, { scrollbarbb.Max.x, scrollbarbb.Min.y + scrollh + scrolloffset }, col, 5 ds );
				GetWindowDrawList( )->PopClipRect( );

				if ( IsMouseClicked( 0 ) && IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) && !obj.scroll_activey ) {
					obj.scroll_activey = true;
					obj.oldscroll = obj.scroll.y;
					obj.clickpos = ( GetIO( ).MousePos.y - scrollbarbb.Min.y ) / scrollbarbb.GetHeight( );
				} if ( !IsMouseDown( 0 ) ) obj.scroll_activey = false;

				if ( obj.scroll_activey ) {
					GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

					float newscroll = ( GetIO( ).MousePos.y - scrollbarbb.Min.y ) / scrollbarbb.GetHeight( );
					float diff = ( newscroll - obj.clickpos ) / invisiblepart;

					obj.scroll.y = obj.oldscroll + diff * GetCurrentWindow( )->ScrollMax.y;
					obj.scroll.y = ImClamp( obj.scroll.y, 0.f, GetCurrentWindow( )->ScrollMax.y );
				}
			}
		}
		// horizontal
		{
			ImRect scrollbarbb{ GetWindowPos( ) + ImVec2{ 3 ds + padding.x ds, GetWindowHeight( ) - 5 ds }, GetWindowPos( ) + GetWindowSize( ) - vec2{ 3 + padding.x, 2 } };
			float visiblepart = GetWindowWidth( ) / ( GetCurrentWindow( )->ContentSize.x + GImGui->Style.WindowPadding.x * 2 );

			if ( visiblepart < 1.f ) {
				float scrollh = scrollbarbb.GetWidth( ) * visiblepart;
				float invisiblepart = 1.f - visiblepart;
				float scrolloffset = ( obj.scroll.x / GetCurrentWindow( )->ScrollMax.x ) * ( scrollbarbb.GetWidth( ) - scrollh );

				obj.xanim = anim( obj.xanim, 0.f, 1.f, IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) || obj.scroll_activex );
				auto col = col_anim( g_style->col( pcol_bg3 ), g_style->col( pcol_text2 ), obj.xanim );

				GetWindowDrawList( )->PushClipRect( scrollbarbb.Min, scrollbarbb.Max );
				GetWindowDrawList( )->AddRectFilled( scrollbarbb.Min + ImVec2{ scrolloffset, 0 }, { scrollbarbb.Min.x + scrollh + scrolloffset, scrollbarbb.Max.y }, col, 5 ds );
				GetWindowDrawList( )->PopClipRect( );

				if ( IsMouseClicked( 0 ) && IsMouseHoveringRect( scrollbarbb.Min, scrollbarbb.Max ) && !obj.scroll_activex ) {
					obj.scroll_activex = true;
					obj.oldscroll = obj.scroll.x;
					obj.clickpos = ( GetIO( ).MousePos.x - scrollbarbb.Min.x ) / scrollbarbb.GetWidth( );
				} if ( !IsMouseDown( 0 ) ) obj.scroll_activex = false;

				if ( obj.scroll_activex ) {
					GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

					float newscroll = ( GetIO( ).MousePos.x - scrollbarbb.Min.x ) / scrollbarbb.GetWidth( );
					float diff = ( newscroll - obj.clickpos ) / invisiblepart;

					obj.scroll.x = obj.oldscroll + diff * GetCurrentWindow( )->ScrollMax.x;
					obj.scroll.x = ImClamp( obj.scroll.x, 0.f, GetCurrentWindow( )->ScrollMax.x );
				}
			}
		}
	}

	GetCurrentWindow( )->Scroll.y = obj.scroll_anim.y < 0.5f ? 0.f : obj.scroll_anim.y > ( GetCurrentWindow( )->ScrollMax.y - 0.5f ) ? GetCurrentWindow( )->ScrollMax.y : ImLerp( GetCurrentWindow( )->Scroll.y, obj.scroll.y, GetIO( ).DeltaTime * 40 );
	GetCurrentWindow( )->Scroll.x = ImLerp( GetCurrentWindow( )->Scroll.x, obj.scroll.x, GetIO( ).DeltaTime * 40 );

	ImGuiWindow* wheeling_window = nullptr;
	if ( GImGui->HoveredWindow ) {
		if ( GImGui->HoveredWindow->Flags & ImGuiWindowFlags_ChildWindow ) {
			for ( ImGuiWindow* window = GImGui->HoveredWindow; window->Flags & ImGuiWindowFlags_ChildWindow; window = window->ParentWindow ) {
				if ( window->ScrollMax[ImGuiAxis_Y] == 0 )
					continue;

				wheeling_window = window;
			}
		} else {
			wheeling_window = GImGui->HoveredWindow;
		}
	}

	if ( wheeling_window == GetCurrentWindow( ) ) {
		if ( !IsKeyDown( ImGuiKey_LeftCtrl ) )
			obj.scroll.y = ImClamp( obj.scroll.y - GetIO( ).MouseWheel * 80, 0.f, GetCurrentWindow( )->ScrollMax.y );
		else
			obj.scroll.x = obj.scroll.x - GetIO( ).MouseWheel * 80;
	}

	obj.scroll_anim.y = ImLerp( obj.scroll_anim.y, obj.scroll.y, GetIO( ).DeltaTime * 40 );
	obj.scroll_anim.x = ImLerp( obj.scroll_anim.x, obj.scroll.x, GetIO( ).DeltaTime * 40 );

	return &obj.scroll.y;
}

// MENU
void c_widgets::widget_menu::begin( const std::string_view& str_id ) {

}
void c_widgets::widget_menu::end( ) {

}
bool c_widgets::widget_menu::button( const ptext& label, ImVec2 size, c_buttonstyle style ) {
	return false;
}

// NOTIFY
void c_widgets::widget_notify::add( const std::string_view& title, const std::string_view& text, notify_ status )
{
	notifications.emplace_back( c_notify{ title, text, status, 3.f } );
}
void c_widgets::widget_notify::handle( )
{
	auto draw_list = GetBackgroundDrawList( );

	float offset = 0.f;
	for ( int i = 0; i < notifications.size( ); ++i ) {
		auto& n = notifications[i];
		float alpha = n.time <= n.fade_time ? n.time / n.fade_time : n.time >= n.duration - n.fade_time ? ( n.duration - n.time ) / n.fade_time : 1.f;

		ImVec2 size{ ImMax( CalcTextSize( n.message.data( ) ).x, CalcTextSize( n.title.data( ) ).x ) + 82 ds, 62 ds };

		if ( n.pos.x == 0 ) n.pos = GetIO( ).DisplaySize - ImVec2{ 0, 20 + offset + size.y };

		n.pos.x = ImLerp( n.pos.x, GetIO( ).DisplaySize.x - size.x - 20, GetIO( ).DeltaTime * 14 );
		n.pos.y = ImLerp( n.pos.y, GetIO( ).DisplaySize.y - 20 - offset - size.y, GetIO( ).DeltaTime * 14 );

		pcolor colors[] = {
			g_style->col( pcol_scheme ).h( 70.f / 172.f ),
			g_style->col( pcol_scheme ).h( 0.14f ),
			g_style->col( pcol_scheme ).h( 0.f ),
			g_style->col( pcol_scheme ).h( 0.59f ),
		};

		draw_list->AddRectFilled( n.pos, n.pos + size, g_style->col( pcol_bg, alpha ), 4 ds );
		draw_list->AddRectFilled( n.pos + vec2{ 40, 6 }, n.pos + size - vec2{ 6, 6 }, g_style->col( pcol_bg2, alpha ), 3 ds );
		draw_list->AddRectFilled( { n.pos.x + 4 ds, n.pos.y + size.y - 1 ds }, { n.pos.x + ( size.x - 8 ds ) * ( n.time / n.duration ), n.pos.y + size.y }, colors[n.status].alpha( alpha ) );

		const char* n_icons[] = {
			"",
			"",
			"",
			"",
		};

		n.time += 1.f / GetIO( ).Framerate;
		offset += size.y + 12;
	}

	notifications.erase(
		std::remove_if( notifications.begin( ), notifications.end( ), []( const c_notify& n ) {
			return n.time >= n.duration;
		} ), 
		notifications.end( )
	);
}

// POPUP
bool c_widgets::widget_popup::begin( const std::string_view& str_id, float alpha, ImVec2 pos, ImVec2 padding )
{
	char temp[64];
	ImFormatString( temp, sizeof( temp ), "%s popup", str_id.data( ) );

	if ( alpha > 0.05f ) {
		if ( pos != ImVec2{ 0, 0 } )
			SetNextWindowPos( pos );
		PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * alpha );
		PushStyleVar( ImGuiStyleVar_WindowPadding, padding );
		PushStyleVar( ImGuiStyleVar_WindowBorderSize, 1 );
		PushStyleVar( ImGuiStyleVar_WindowRounding, 4 ds );
		Begin( temp, 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize );

		if ( alpha > 0.95f ) {
			GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		} else {
			GetCurrentWindow( )->Flags &= ~ImGuiWindowFlags_NoBringToFrontOnFocus;
		}

		return true;
	}

	return false;
}
void c_widgets::widget_popup::end( )
{
	End( );
	PopStyleVar( 4 );
}

bool c_widgets::widget_nav::subtab( const c_subtab& item, bool selected )
{
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( item.label.str.data( ), 0, s{ } );
    
    auto* window = GetCurrentWindow( );
    auto id = window->GetID( item.label.str.data( ) );
    ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ CalcTextSize( item.label.translate( ).data( ), 0, 1 ).x + 24, 16 ds } };
    ItemSize( bb );
    ItemAdd( bb, id );
    
    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );
    
    window->DrawList->AddText( bb.Min + vec2{ 24, 0 }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.selected ), item.label.translate( ).data( ), FindRenderedTextEnd( item.label.translate( ).data( ) ) );
    g_draw->text( icons, 16, bb.Min + vec2{ 0, 0 }, g_style->col( pcol_text2 ), item.icon );
    
    return pressed;
}

bool c_widgets::widget_nav::tab( c_tab tab, int num, bool selected )
{
    auto* window = GetCurrentWindow( );
    
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( tab.label.str.data( ), 0, s{ } );
    
    bool pressed = InvisibleButton( tab.label.str.data( ), { 36 ds + ( 92 ds ) * slate->sidebar_anim, 36 ds } );
    bool hovered = IsItemHovered( ), held = IsItemActive( );
    ImRect bb = GImGui->LastItemData.Rect;
    
    obj.hover = anim( obj.hover, 0.f, 1.f, hovered && !selected );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );
    
    window->DrawList->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg3, 0.5f * obj.anim + 0.5f * obj.selected ), 4 ds );
#if defined(BRAND_MODERN_UI)
    if ( obj.selected > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, ImVec2( bb.Min.x + 3.f, bb.Max.y ), g_style->col( pcol_scheme ), 3.f );
#endif
    g_draw->text( icons, int( 18.f - 2.f * slate->sidebar_anim ) * 1.f, bb.Min + vec2{ 8, 8 + 2.f * slate->sidebar_anim }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_scheme ), obj.selected ), tab.icon );
    g_draw->text( ttsupermolotneuetrl_md, 16, bb.Min + vec2{ 34 + 24 * ImPow( ( 1.f - slate->sidebar_anim ), ImMax( num * 0.5f, 1.f ) ), 10 }, col_anim( col_anim( g_style->col( pcol_text2, slate->sidebar_anim ), g_style->col( pcol_text3, slate->sidebar_anim ), obj.hover ), g_style->col( pcol_text, slate->sidebar_anim ), obj.selected ), tab.label.translate( ).data( ), FindRenderedTextEnd( tab.label.translate( ).data( ) ) );
    
    return pressed;
}

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
bool c_widgets::widget_nav::tab_trinity( c_tab tab, bool selected, float width )
{
    auto* window = GetCurrentWindow( );

    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( tab.label.str.data( ), 100, s{ } );

    const float itemH = 30.f;
    const float itemW = width - 12.f;
    SetCursorPosX( 6.f );
    bool pressed = InvisibleButton( tab.label.str.data( ), { itemW, itemH } );
    bool hovered = IsItemHovered( );
    ImRect bb = GImGui->LastItemData.Rect;

    obj.hover = anim( obj.hover, 0.f, 1.f, hovered && !selected );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

    const ImU32 accent = IM_COL32( BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255 );

    if ( obj.selected > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, 12 ), 3.f );
    else if ( obj.hover > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, 6 ), 3.f );

    if ( obj.selected > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, ImVec2{ bb.Min.x + 2.f, bb.Max.y }, accent, 1.f );

    g_draw->text( icons, 14, bb.Min + ImVec2{ 10.f, 8.f }, col_anim( g_style->col( pcol_text3 ), accent, obj.selected ), tab.icon );

    const char* title = tab.label.translate( ).data( );
    const ImU32 titleCol = col_anim( IM_COL32( 128, 134, 146, 255 ), IM_COL32( 232, 235, 242, 255 ), obj.selected );
    window->DrawList->AddText( bb.Min + ImVec2{ 32.f, 7.f }, titleCol, title, FindRenderedTextEnd( title ) );

    return pressed;
}

void c_widgets::widget_nav::drawtabs_trinity( float width )
{
    BeginGroup( );
    {
        for ( int i = 0; i < tabs.size( ); ++i ) {
            if ( tab_trinity( tabs[i], next == i, width ) && next != i ) {
                next = i;
                tab_animdest = 0;
            }
            widgets->spacing( 1 ds );
        }
    }
    EndGroup( );

    tab_anim = ImLerp( tab_anim, tab_animdest, GetIO( ).DeltaTime * 17 );
    subtab_anim = ImLerp( subtab_anim, subtab_animdest, GetIO( ).DeltaTime * 17 );

    if ( tab_anim < 0.05f ) {
        tab_animdest = 1.f;
        current = next;
    }

    if ( subtab_anim < 0.05f ) {
        subtab_animdest = 1.f;
        tabs[current].current = tabs[current].next;
    }
}

bool c_widgets::widget_nav::tab_hz( const c_tab& tab, bool selected )
{
    auto* window = GetCurrentWindow( );

    struct s {
        float hover;
        float selected;
    }; auto& obj = anim_obj( tab.label.str.data( ), 310, s{ } );

    const float box = 40.f;
    bool pressed = InvisibleButton( tab.label.str.data( ), { box, box } );
    bool hovered = IsItemHovered( );
    ImRect bb = GImGui->LastItemData.Rect;

    obj.hover = anim( obj.hover, 0.f, 1.f, hovered && !selected );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );

    const ImU32 accent = IM_COL32( BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255 );
    if ( obj.selected > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, ( int )( 18.f + 10.f * obj.selected ) ), 10.f );
    else if ( obj.hover > 0.02f )
        window->DrawList->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, ( int )( 12.f * obj.hover ) ), 10.f );

    const ImU32 iconCol = col_anim( IM_COL32( 140, 146, 158, 255 ), accent, obj.selected );
    if ( fonts[icons].get( 16 ) )
        g_draw->text( icons, 16, bb.Min + ImVec2{ 12.f, 12.f }, iconCol, tab.icon );
    return pressed;
}

void c_widgets::widget_nav::drawtabs_hz( )
{
    BeginGroup( );
    {
        for ( int i = 0; i < ( int )tabs.size( ); ++i ) {
            if ( tab_hz( tabs[i], next == i ) && next != i ) {
                next = i;
                tab_animdest = 0;
            }
            if ( i + 1 < ( int )tabs.size( ) )
                SameLine( 0, 6.f );
        }
    }
    EndGroup( );

    const float t = ImClamp( GetIO( ).DeltaTime * 17.f, 0.f, 1.f );
    tab_anim = ImLerp( tab_anim, tab_animdest, t );
    subtab_anim = ImLerp( subtab_anim, subtab_animdest, t );

    if ( tab_anim < 0.05f ) {
        tab_animdest = 1.f;
        current = next;
    }

    if ( subtab_anim < 0.05f ) {
        subtab_animdest = 1.f;
        if ( current >= 0 && current < ( int )tabs.size( ) )
            tabs[current].current = tabs[current].next;
    }
}

bool c_widgets::widget_nav::subtab_trinity( const c_subtab& item, bool selected )
{
    struct s {
        float anim;
        float hover;
        float selected;
    }; auto& obj = anim_obj( item.label.str.data( ), 200, s{ } );

    auto* window = GetCurrentWindow( );
    auto id = window->GetID( item.label.str.data( ) );
    const char* label = item.label.translate( ).data( );
    ImVec2 textSize = CalcTextSize( label, 0, true );
    ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + ImVec2{ textSize.x + 20.f, 29.f } };
    ItemSize( bb );
    ItemAdd( bb, id );

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held );

    obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
    obj.selected = anim( obj.selected, 0.f, 1.f, selected );
    obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

    const ImU32 accent = IM_COL32( BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255 );
    if ( obj.hover > 0.02f && obj.selected < 0.98f )
        window->DrawList->AddRectFilled( bb.Min, bb.Max, IM_COL32( 255, 255, 255, 6 ), 4.f );

    window->DrawList->AddText( bb.Min + ImVec2{ 10.f, ( bb.GetHeight( ) - textSize.y ) * 0.5f - 1.f }, col_anim( IM_COL32( 140, 146, 158, 255 ), IM_COL32( 245, 245, 247, 255 ), obj.selected ), label, FindRenderedTextEnd( label ) );

    if ( obj.selected > 0.02f ) {
        const float trackW = bb.GetWidth( ) - 10.f;
        const float lineW = trackW * obj.selected;
        const ImVec2 lineMin{ bb.Min.x + 5.f + ( trackW - lineW ) * 0.5f, bb.Max.y - 3.f };
        window->DrawList->AddRectFilled( lineMin, ImVec2{ lineMin.x + lineW, bb.Max.y - 1.f }, accent, 1.5f );
    }

    return pressed;
}

void c_widgets::widget_nav::drawsubtabs_trinity( )
{
    if ( tabs[current].subtabs.empty( ) )
        return;

    PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * tab_anim );
    BeginGroup( );
    {
        for ( int i = 0; i < tabs[current].subtabs.size( ); ++i ) {
            if ( subtab_trinity( tabs[current].subtabs[i], i == tabs[current].next ) && i != tabs[current].next ) {
                tabs[current].next = i;
                subtab_animdest = 0;
            }
            SameLine( 0, 2 );
        }
    }
    EndGroup( );
    PopStyleVar( );
}
#endif

void c_widgets::widget_nav::drawtabs( )
{
    SetCursorPosX( 19 ds );
	widgets->spacing( 28 ds );
    BeginGroup( );
    {
        for ( int i = 0; i < tabs.size( ); ++i ) {
            if ( tab( tabs[i], i + 1, next == i ) && next != i ) {
                next = i;
                tab_animdest = 0;
            }
            
            widgets->spacing( 12 ds );
        }
    }
    EndGroup( );
    
    
    tab_anim = ImLerp( tab_anim, tab_animdest, GetIO( ).DeltaTime * 17 );
    subtab_anim = ImLerp( subtab_anim, subtab_animdest, GetIO( ).DeltaTime * 17 );
    
    if ( tab_anim < 0.05f ) {
        tab_animdest = 1.f;
        current = next;
    }
    
    if ( subtab_anim < 0.05f ) {
        subtab_animdest = 1.f;
        tabs[current].current = tabs[current].next;
    }
}
void c_widgets::widget_nav::drawsubtabs( )
{
    if ( tabs[current].subtabs.empty( ) ) {
        return;
    }
    
	static float w = 0;
    PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * tab_anim );
    SetCursorPos( { GetWindowWidth( ) / 2 - w / 2, 20 } );
	PushStyleVar( ImGuiStyleVar_WindowPadding, { 12, 8 } );
	PushStyleVar( ImGuiStyleVar_ChildRounding, 999 );
    BeginChild( "subtabs", { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
    {
        for ( int i = 0; i < tabs[current].subtabs.size( ); ++i ) {
            if ( subtab( tabs[current].subtabs[i], i == tabs[current].next ) && i != tabs[current].next ) {
                tabs[current].next = i;
                subtab_animdest = 0;
            }
            SameLine( 0, 8 ds );
        }

		w = GetWindowWidth( );
    }
    EndChild( );
	PopStyleVar( 2 );
	widgets->spacing( 0 );
    
    PopStyleVar( );
}
void c_widgets::widget_nav::drawpage( )
{
	if ( tabs[current].pages.size( ) <= tabs[current].current )
		return;

	tabs[current].pages[tabs[current].current]( );
}
void c_widgets::widget_nav::addpage( int tab, std::function< void( ) > code )
{
	tabs[tab].pages.push_back( code );
}

// COLORPICKER

bool c_widgets::widget_colorpicker::huebar( const char* str_id, float* h, float s, float v )
{
	float h_values[] {
		0.f, 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f, 1.f
	};
	int h_size = IM_ARRAYSIZE( h_values );

	ImVec2 size{ 207 ds, int( 5 ds ) * 1.f };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	for ( int i = 0; i < h_size - 1; ++i ) {
		ImColor col1, col2;

		ColorConvertHSVtoRGB( h_values[i], ImClamp( s, 0.6f, 1.f ), ImClamp( v, 0.6f, 1.f ), col1.Value.x, col1.Value.y, col1.Value.z );
		ColorConvertHSVtoRGB( h_values[i + 1], ImClamp( s, 0.6f, 1.f ), ImClamp( v, 0.6f, 1.f ), col2.Value.x, col2.Value.y, col2.Value.z );
		col1.Value.w = col2.Value.w = GImGui->Style.Alpha;
		
		g_draw->gradient( GetWindowDrawList( ), { bb.Min.x + ( size.x / ( h_size - 1 ) ) * i, bb.Min.y }, { bb.Min.x + ( size.x / ( h_size - 1 ) ) * ( i + 1 ), bb.Max.y }, col1, col2, col2, col1, 4, i == 0 ? ImDrawFlags_RoundCornersLeft : ( i == h_size - 2 ) ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone );
	}

	GetWindowDrawList( )->AddCircle( { bb.Min.x + size.x * *h, bb.GetCenter( ).y }, 3 ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha } );

	InvisibleButton( "hue", size );
	if ( IsItemActive( ) ) {
		*h = ImSaturate( ( GetIO( ).MousePos.x - bb.Min.x ) / size.x );

		return true;
	}

	return false;
}
bool c_widgets::widget_colorpicker::alphabar( float* col )
{
	ImVec2 size{ 6 ds, 132 ds };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	float square_sz = 3 ds;

	GetWindowDrawList( )->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	for ( int i = 0; i < size.y / ( square_sz ) - 1; ++i ) {
		GetWindowDrawList( )->AddRectFilled( bb.Min + ImVec2{ 0, square_sz * i }, bb.Min + ImVec2{ bb.GetWidth( ) / 2, square_sz * ( i + 1 ) }, ( i + 1 ) % 2 == 0 ? ImColor{ 222, 222, 222, int( 255 * GImGui->Style.Alpha ) } : ImColor{ 155, 155, 155, int( 255 * GImGui->Style.Alpha ) }, 4, i == 0 ? ImDrawFlags_RoundCornersTopLeft : ( i == size.y / square_sz - 2 ) ? ImDrawFlags_RoundCornersBottomLeft : ImDrawFlags_RoundCornersNone );
		GetWindowDrawList( )->AddRectFilled( bb.Min + ImVec2{ bb.GetWidth( ) / 2, square_sz * i }, bb.Min + ImVec2{ bb.GetWidth( ), square_sz * ( i + 1 ) }, ( i + 1 ) % 2 != 0 ? ImColor{ 222, 222, 222, int( 255 * GImGui->Style.Alpha ) } : ImColor{ 155, 155, 155, int( 255 * GImGui->Style.Alpha ) }, 4, i == 0 ? ImDrawFlags_RoundCornersTopRight : ( i == size.y / square_sz - 2 ) ? ImDrawFlags_RoundCornersBottomRight : ImDrawFlags_RoundCornersNone );
	}

	g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, ImColor{ col[0], col[1], col[2], GImGui->Style.Alpha }, ImColor{ col[0], col[1], col[2], 0.f }, ImColor{ col[0], col[1], col[2], 0.f }, 4 );
	GetWindowDrawList( )->Flags |= ImDrawListFlags_AntiAliasedFill;

	GetWindowDrawList( )->AddCircleFilled( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha }, 30 );
	GetWindowDrawList( )->AddCircleFilled( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ col[0], col[1], col[2], col[3] * GImGui->Style.Alpha }, 30 );
	GetWindowDrawList( )->AddCircle( { bb.GetCenter( ).x, bb.Min.y + ( 1.f - col[3] ) * size.y }, 3.5f ds, ImColor{ 1.f, 1.f, 1.f, GImGui->Style.Alpha }, 30 );

	InvisibleButton( "a", size );
    if ( IsItemActive( ) )
    {
        col[3] = 1.f - ImSaturate( ( GetIO( ).MousePos.y - bb.Min.y ) / size.y );

        return true;
    }

	return false;
}
bool c_widgets::widget_colorpicker::square( const char* str_id, float h, float* s, float* v )
{
	ImVec2 size{ 193 ds, 132 ds };
	ImRect bb{ GetCurrentWindow( )->DC.CursorPos, GetCurrentWindow( )->DC.CursorPos + size };

	ImColor col_white{ 1.f, 1.f, 1.f, GImGui->Style.Alpha };
	ImColor col_black{ 0.f, 0.f, 0.f, GImGui->Style.Alpha };
	ImColor col_hue;

	ColorConvertHSVtoRGB( h, 1, 1, col_hue.Value.x, col_hue.Value.y, col_hue.Value.z );
	col_hue.Value.w = GImGui->Style.Alpha;

	GetWindowDrawList( )->Flags &= ~ImDrawListFlags_AntiAliasedFill;
	g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, col_white, col_hue, col_hue, col_white, 4 ds, 0 );
    g_draw->gradient( GetWindowDrawList( ), bb.Min, bb.Max, 0, 0, col_black, col_black, 4 ds, 0 );
	GetWindowDrawList( )->Flags |= ImDrawListFlags_AntiAliasedFill;

	GetWindowDrawList( )->AddCircle( bb.Min + size * ImVec2{ *s, 1.f - *v }, 3 ds, col_white, 36 );

	InvisibleButton( "sv", size );
    if ( IsItemActive( ) )
    {
        *s = ImSaturate( ( GetIO( ).MousePos.x - bb.Min.x ) / size.x );
        *v = 1.f - ImSaturate( ( GetIO( ).MousePos.y - bb.Min.y ) / size.y );

        return true;
    }
}
bool c_widgets::widget_colorpicker::draw( const std::string_view& str_id, float* col )
{
	bool value_changed = false;

	struct s {
		float h, s, v;
		bool init;
	}; auto& obj = anim_obj( str_id.data( ), 2323321, s{ } );

	if ( !obj.init ) {
		ColorConvertRGBtoHSV( col[0], col[1], col[2], obj.h, obj.s, obj.v );
		obj.init = true;
	}
	
	BeginGroup( );
	value_changed |= square( str_id.data( ), obj.h, &obj.s, &obj.v );
	SameLine( );
	value_changed |= alphabar( col );
	EndGroup( );
	value_changed |= huebar( str_id.data( ), &obj.h, obj.s, obj.v );

	static char buf[7];
	static char alpha_buf[7];
	ImFormatString( buf, sizeof( buf ), "%02X%02X%02X", int( col[0] * 255 ), int( col[1] * 255 ), int( col[2] * 255 ) );
	ImFormatString( alpha_buf, sizeof( alpha_buf ), "%d%%", int( col[3] * 100 ) );

	PushStyleColor( ImGuiCol_FrameBg, GetColorU32( ImGuiCol_FrameBgHovered ) );
	PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1 );
	PushStyleVar( ImGuiStyleVar_FramePadding, { 6, 3 } );
	PushItemFlag( ImGuiItemFlags_NoNav, true );

	if ( value_changed ) {
		ColorConvertHSVtoRGB( obj.h, obj.s, obj.v, col[0], col[1], col[2] );
	}

	bool hex_changed = widgets->textinput( "##hex_input", buf, sizeof( buf ), { .padding = vec2{ 8, 8 }, .size = vec2{ 117, 0 } } );
	SameLine( 0, 10 ds );

	bool alpha_changed = widgets->textinput( "##a_input", alpha_buf, sizeof( alpha_buf ), { .padding = vec2{ 8, 8 }, .size = vec2{ 80, 0 } } );

	PopItemFlag( );
	PopStyleVar( 2 );
	PopStyleColor( );

	int i[4];
	sscanf( buf, "%02X%02X%02X", ( unsigned int* )&i[0], ( unsigned int* )&i[1], ( unsigned int* )&i[2] );
	sscanf( alpha_buf, "%d%%", ( unsigned int* )&i[3] );
	if ( hex_changed ) {
		col[0] = i[0] / 255.f;
		col[1] = i[1] / 255.f;
		col[2] = i[2] / 255.f;

		ColorConvertRGBtoHSV( col[0], col[1], col[2], obj.h, obj.s, obj.v );
	}
	
	if ( alpha_changed ) {
		col[3] = i[3] / 100.f;
	}

	return value_changed;
}
bool c_widgets::widget_colorpicker::colorbutton( const char* str_id, const ImColor& color )
{
	struct s {
		float anim = 0;
		float hover = 0;
		float held = 0;
	}; auto& obj = anim_obj( str_id, 0, s{ } );

	auto window = GetCurrentWindow( );
	auto id = window->GetID( str_id );
	ImRect bb{ window->DC.CursorPos, window->DC.CursorPos + vec2{ 16, 16 } };
	ItemSize( bb );
	ItemAdd( bb, id );

	bool hovered, held;
	bool pressed = ButtonBehavior( bb, id, &hovered, &held );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.held = anim( obj.held, 0.f, 1.f, held );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || held );

	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, g_style->col( pcol_bg3 ), bb.GetSize( ).x * 5 );

	window->DrawList->PushClipRect( bb.GetCenter( ), bb.Max );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, pcolor{ 255, 255, 255 }, bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->PushClipRect( bb.Min, { bb.GetCenter( ).x, bb.Max.y } );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, ImColor{ color.Value.x, color.Value.y, color.Value.z, GImGui->Style.Alpha }, bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->PushClipRect( { bb.GetCenter( ).x, bb.Min.y }, bb.Max );
	window->DrawList->AddCircleFilled( bb.GetCenter( ), bb.GetSize( ).x / 2, ImColor{ color.Value.x, color.Value.y, color.Value.z, GImGui->Style.Alpha * color.Value.w }, bb.GetSize( ).x * 5 );
	window->DrawList->PopClipRect( );

	window->DrawList->AddCircle( bb.GetCenter( ), bb.GetSize( ).x / 2 - 1, pcolor{ 0, 0, 0, 0.3f }, bb.GetSize( ).x * 5, 2.5f );

	return pressed;
}

// MODAL
void c_widgets::widget_modal::add( std::function< void( ) > code )
{
	modals.push_back( { code } );
}
void c_widgets::widget_modal::close( )
{
	modals[modals.size( ) - 1].anim_dest = 0;
}
void c_widgets::widget_modal::handle( )
{
	int i = 0;
	for ( auto& modal : modals ) {
		modal.anim = ImLerp( modal.anim, modal.anim_dest, GetIO( ).DeltaTime * 17 );

		if ( modal.anim < 0.05f && modal.anim_dest == 0 ) {
			modals.erase( modals.begin( ) + ( modals.size( ) - 1 ) );
		}

		SetCursorPos( vec2{ 8, 8 } );
		char temp[64];
		ImFormatString( temp, sizeof( temp ), "modal %d", i );
		PushStyleVar( ImGuiStyleVar_Alpha, GImGui->Style.Alpha * modal.anim );
		PushStyleColor( ImGuiCol_ChildBg, GetColorU32( ImGuiCol_WindowBg, 0.92f ) );
		BeginChild( temp, -vec2{ 8, 8 }, 0, ImGuiWindowFlags_NoBackground );
		{
			PopStyleColor( );

			GetWindowDrawList( )->AddRectFilled( GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), GetColorU32( ImGuiCol_ChildBg ), GImGui->Style.ChildRounding );
			GetWindowDrawList( )->AddRect( GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), GetColorU32( ImGuiCol_Border ), GImGui->Style.ChildRounding );

			if ( IsWindowHovered( ) && IsMouseClicked( 0 ) ) {
				close( );
			}

			if ( modal.code )
				modal.code( );
		}
		EndChild( );
		PopStyleVar( );

		i++;
	}
}

// BINDER

void c_widgets::widget_binder::popup( const std::string_view& label, void* v, ht_ type, float min, float max, std::vector< std::string_view > items )
{
	
}

// TABLE

void c_widgets::widget_table::begin( const char* str_id, const std::vector< ptext >& columns )
{
	this->id = str_id;
	this->columns = columns;
	this->colsizes.resize( columns.size( ) );
	
	float wp = GImGui->Style.WindowPadding.x;

	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
	BeginChild( str_id, { GetWindowWidth( ) - wp * 2, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
	PopStyleVar( );
	PopStyleColor( );

	char temp[64];
	ImFormatString( temp, sizeof( temp ), "%s header", str_id );
	PushStyleVar( ImGuiStyleVar_WindowPadding, { GImGui->Style.WindowPadding.x, 11 ds } );
	BeginChild( temp, vec2{ 0, 36 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground );
	{
		for ( int i = 0; i < columns.size( ); ++i ) {
			colsizes[i] = ImMax( CalcTextSize( columns[i].translate( ).data( ) ).x, colsizes[i] );
			Text( columns[i].translate( ).data( ) );

			if ( i == columns.size( ) - 2 ) {
				SameLine( GetWindowWidth( ) - GImGui->Style.WindowPadding.x - CalcTextSize( columns[i].translate( ).data( ) ).x );
			} else {
				SameLine( 0, colsizes[i] - CalcTextSize( columns[i].translate( ).data( ) ).x + gap ds );
			}
		}
	}
	EndChild( );
	PopStyleVar( );
	widgets->spacing( 0 );

	ImFormatString( temp, sizeof( temp ), "%s content", str_id );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border );
}
void c_widgets::widget_table::end( )
{
	EndChild( );
	EndChild( );
	currow = 0;
}

void c_widgets::widget_table::beginrow( )
{
	char temp[128];
	ImFormatString( temp, sizeof( temp ), "%s%d", id, currow );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
}
void c_widgets::widget_table::endrow( )
{
	EndChild( );
	currow++;
	curcol = 0;
	colpos = 0;
}

void c_widgets::widget_table::begincolumn( float w )
{
	char temp[128];
	ImFormatString( temp, sizeof( temp ), "%s%d%d", id, currow, curcol );
	BeginChild( temp, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
	if ( curcol == columns.size( ) - 1 && w < colsizes[curcol] ) {
		SetCursorPosX( colsizes[curcol] - w );
	}
}
void c_widgets::widget_table::endcolumn( )
{
	colsizes[curcol] = ImMax( colsizes[curcol], GetCurrentWindow( )->ContentSize.x );
	float cellw = GetCurrentWindow( )->ContentSize.x;
	EndChild( );
	if ( curcol == columns.size( ) - 2 ) {
		SameLine( GetWindowWidth( ) - colsizes[curcol + 1] );
	} else {
		SameLine( 0, colsizes[curcol] - cellw + gap ds );
	}
	curcol++;
}

void c_widgets::widget_table::setcolumnsizes( const std::vector< float >& sizes )
{
	for ( int i = 0; i < sizes.size( ); ++i ) {
		colsizes[i] = sizes[i];
	}
}

// TABBAR

c_nav& c_widgets::c_tabbar::draw( const char* str_id, const std::vector< const char* > list )
{
	PushStyleColor( ImGuiCol_ChildBg, g_style->col( pcol_bg3 ).vec4( ) );
	PushStyleVar( ImGuiStyleVar_WindowPadding, vec2{ 2, 2 } );
	BeginChild( str_id, { 0, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding );
	{
		for ( int i = 0; i < list.size( ); ++i ) {
			if ( tab( list[i], nav.next == i ) && nav.next != i ) {
				nav.next = i;
				nav.animdest = 0;
			}
			SameLine( 0, 2 ds );
		}
	}
	EndChild( );
	PopStyleVar( );
	PopStyleColor( );

	nav.handle( );
	return nav;
}
bool c_widgets::c_tabbar::tab( const ptext& label, bool selected )
{
	ImGuiWindow* window = GetCurrentWindow( );
	bool pressed = InvisibleButton( label.translate( ).data( ), CalcTextSize( label.translate( ).data( ), 0, 1 ) + GImGui->Style.FramePadding * 2 );
	bool hovered = IsItemHovered( ), held = IsItemActive( );
	ImRect bb = GImGui->LastItemData.Rect;

	struct s {
		float anim = 0;
		float hover = 0;
		float selected = 0;
	}; auto& obj = anim_obj( label.translate( ).data( ), 123444320, s{ } );

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.selected = anim( obj.selected, 0.f, 1.f, selected );
	obj.anim = anim( obj.anim, 0.f, 1.f, hovered || selected );

	window->DrawList->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg2, obj.selected ), GImGui->Style.FrameRounding );

	auto col = col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text2 ), obj.hover ), g_style->col( pcol_text ), obj.selected );
	window->DrawList->AddText( bb.Min + GImGui->Style.FramePadding, col, label.translate( ).data( ), FindRenderedTextEnd( label.translate( ).data( ) ) );

	return pressed;
}

// custom widgets