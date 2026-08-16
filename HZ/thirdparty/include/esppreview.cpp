#define NOMINMAX
#include "slate.h"
#include <DirectXMath.h>
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "../../Render/Overlay.hpp"
#include "../../Definations/Brand.hpp"

using namespace ImGui;

using namespace DirectX;

void c_esppreview::create_text( const std::string& name, const std::string& label, e_esp_item_pos pos, bool* visible, float* col )
{
	c_esp_text* item = new c_esp_text( espitem_text, name, pos, visible, col );
	item->label = label;
	items.emplace_back( item );
}
void c_esppreview::create_bar( const std::string& name, e_esp_item_pos pos, bool* visible, float* col, bool* rounded )
{
	c_esp_bar* item = new c_esp_bar( espitem_bar, name, pos, visible, col );
	item->rounded = rounded;
	items.emplace_back( item );
}

std::vector< c_esp_base_item* > c_esppreview::get_items_by_pos( e_esp_item_pos pos )
{
	std::vector< c_esp_base_item* > result;
	for ( auto& item : items )
	{
		if ( item->pos != pos )
			continue;

		result.push_back( item );
	}
	return result;
}

// /////////////////////////////////////////////////////////

static void draw_preview_skeleton( const ImRect& box_bb, bool* skeleton_enabled, float* skeleton_col )
{
	if ( !skeleton_enabled || !*skeleton_enabled || !skeleton_col )
		return;

	const float w = box_bb.GetWidth( );
	const float h = box_bb.GetHeight( );

	auto pt = [&]( float x, float y ) -> ImVec2 {
		return { box_bb.Min.x + w * x, box_bb.Min.y + h * y };
	};

	// Tuned for s_f_y_hooker_03 transparent cutout (266x551).
	const ImVec2 neck = pt( 0.50f, 0.10f );
	const ImVec2 pelvis = pt( 0.50f, 0.46f );
	const ImVec2 l_clav = pt( 0.42f, 0.14f );
	const ImVec2 r_clav = pt( 0.58f, 0.14f );
	const ImVec2 l_upper = pt( 0.34f, 0.24f );
	const ImVec2 r_upper = pt( 0.66f, 0.24f );
	const ImVec2 l_fore = pt( 0.26f, 0.34f );
	const ImVec2 r_fore = pt( 0.74f, 0.34f );
	const ImVec2 l_hand = pt( 0.20f, 0.42f );
	const ImVec2 r_hand = pt( 0.80f, 0.42f );
	const ImVec2 l_thigh = pt( 0.45f, 0.62f );
	const ImVec2 r_thigh = pt( 0.55f, 0.62f );
	const ImVec2 l_calf = pt( 0.44f, 0.76f );
	const ImVec2 r_calf = pt( 0.56f, 0.76f );
	const ImVec2 l_foot = pt( 0.43f, 0.96f );
	const ImVec2 r_foot = pt( 0.57f, 0.96f );

	const pcolor col{ skeleton_col[0], skeleton_col[1], skeleton_col[2], skeleton_col[3] };
	auto bone = [&]( const ImVec2& a, const ImVec2& b ) {
		GetWindowDrawList( )->AddLine( a, b, col, 1.5f );
	};

	bone( neck, r_clav ); bone( neck, l_clav );
	bone( r_clav, r_upper ); bone( l_clav, l_upper );
	bone( r_upper, r_fore ); bone( l_upper, l_fore );
	bone( r_fore, r_hand ); bone( l_fore, l_hand );
	bone( neck, pelvis );
	bone( pelvis, l_thigh ); bone( pelvis, r_thigh );
	bone( l_thigh, l_calf ); bone( r_thigh, r_calf );
	bone( l_calf, l_foot ); bone( r_calf, r_foot );
}

static bool file_exists( const std::string& path )
{
	std::ifstream f( path, std::ios::binary );
	return f.good( );
}

void c_esppreview::ensure_ped_preview_texture( )
{
	if ( ped_preview_load_attempted )
		return;

	ped_preview_load_attempted = true;

	auto* device = FrameWork::Overlay::dxGetDevice( );
	if ( !device )
		return;

	const std::string ped_file = "s_f_y_hooker_03.png";
	std::vector< std::string > candidates = {
		std::string( BRAND_DATA_ROOT ) + "esp_preview\\" + ped_file,
		std::string( BRAND_CLIENT_ROOT ) + "esp_preview\\" + ped_file,
		std::string( BRAND_DATA_ROOT ) + ped_file,
	};

	char module_path[MAX_PATH]{};
	if ( GetModuleFileNameA( nullptr, module_path, MAX_PATH ) )
	{
		std::string exe_dir = module_path;
		const auto slash = exe_dir.find_last_of( "\\/" );
		if ( slash != std::string::npos )
			exe_dir.resize( slash + 1 );

		candidates.push_back( exe_dir + "Data\\esp_preview\\" + ped_file );
		candidates.push_back( exe_dir + "esp_preview\\" + ped_file );
	}

	for ( const auto& path : candidates )
	{
		if ( !file_exists( path ) )
			continue;

		if ( SUCCEEDED( D3DX11CreateShaderResourceViewFromFileA( device, path.c_str( ), nullptr, nullptr, &ped_preview_srv, nullptr ) ) )
			break;
	}
}

void c_esppreview::draw_ped_preview( const ImRect& box_bb )
{
	ensure_ped_preview_texture( );

	if ( ped_preview_srv )
	{
		GetWindowDrawList( )->AddImage(
			( ImTextureID )ped_preview_srv,
			box_bb.Min,
			box_bb.Max,
			ImVec2{ 0, 0 },
			ImVec2{ 1, 1 },
			IM_COL32( 255, 255, 255, 255 )
		);
		return;
	}

	// Fallback silhouette if the ped texture is unavailable.
	const ImVec2 c = box_bb.GetCenter( );
	const float w = box_bb.GetWidth( ) * 0.34f;
	const float head_r = w * 0.22f;
	GetWindowDrawList( )->AddCircleFilled( { c.x, box_bb.Min.y + box_bb.GetHeight( ) * 0.10f }, head_r, IM_COL32( 45, 45, 45, 200 ), 24 );
	GetWindowDrawList( )->AddRectFilled(
		{ c.x - w * 0.55f, box_bb.Min.y + box_bb.GetHeight( ) * 0.18f },
		{ c.x + w * 0.55f, box_bb.Min.y + box_bb.GetHeight( ) * 0.56f },
		IM_COL32( 45, 45, 45, 200 ),
		6 ds
	);
	GetWindowDrawList( )->AddRectFilled(
		{ c.x - w * 0.22f, box_bb.Min.y + box_bb.GetHeight( ) * 0.56f },
		{ c.x - w * 0.05f, box_bb.Min.y + box_bb.GetHeight( ) * 0.93f },
		IM_COL32( 45, 45, 45, 200 ),
		4 ds
	);
	GetWindowDrawList( )->AddRectFilled(
		{ c.x + w * 0.05f, box_bb.Min.y + box_bb.GetHeight( ) * 0.56f },
		{ c.x + w * 0.22f, box_bb.Min.y + box_bb.GetHeight( ) * 0.93f },
		IM_COL32( 45, 45, 45, 200 ),
		4 ds
	);
}

static void draw_box( ImRect bb, bool* enabled, float* col, int* box_type )
{
	if ( !*enabled )
		return;

	if ( *box_type == 0 ) // Normal box
	{
		GetWindowDrawList( )->AddRect( bb.Min, bb.Max, pcolor{ 0.f, 0.f, 0.f, 0.8f }, 2 ds, 0, 3 ds );
		GetWindowDrawList( )->AddRect( bb.Min, bb.Max, pcolor{ col[0], col[1], col[2], col[3] }, 2 ds );
	}
	else if ( *box_type == 1 ) // Corner box
	{
		float Width = bb.GetWidth();
		float Height = bb.GetHeight();
		float cs = ((Height / 2.f / 100.f) + (Width / 2.f / 100.f)) / 2.f * 30.f;
		
		// Top-left corner
		ImVec2 TL[] = { {bb.Min.x, bb.Min.y + cs}, {bb.Min.x, bb.Min.y}, {bb.Min.x + cs, bb.Min.y} };
		// Top-right corner
		ImVec2 TR[] = { {bb.Max.x, bb.Min.y + cs}, {bb.Max.x, bb.Min.y}, {bb.Max.x - cs, bb.Min.y} };
		// Bottom-left corner
		ImVec2 BL[] = { {bb.Min.x, bb.Max.y - cs}, {bb.Min.x, bb.Max.y}, {bb.Min.x + cs, bb.Max.y} };
		// Bottom-right corner
		ImVec2 BR[] = { {bb.Max.x, bb.Max.y - cs}, {bb.Max.x, bb.Max.y}, {bb.Max.x - cs, bb.Max.y} };
		
		// Draw outline
		GetWindowDrawList()->AddPolyline(TL, 3, pcolor{ 0.f, 0.f, 0.f, 0.8f }, ImDrawFlags_None, 3 ds);
		GetWindowDrawList()->AddPolyline(TR, 3, pcolor{ 0.f, 0.f, 0.f, 0.8f }, ImDrawFlags_None, 3 ds);
		GetWindowDrawList()->AddPolyline(BL, 3, pcolor{ 0.f, 0.f, 0.f, 0.8f }, ImDrawFlags_None, 3 ds);
		GetWindowDrawList()->AddPolyline(BR, 3, pcolor{ 0.f, 0.f, 0.f, 0.8f }, ImDrawFlags_None, 3 ds);
		
		// Draw colored corners
		GetWindowDrawList()->AddPolyline(TL, 3, pcolor{ col[0], col[1], col[2], col[3] }, ImDrawFlags_None, 2 ds);
		GetWindowDrawList()->AddPolyline(TR, 3, pcolor{ col[0], col[1], col[2], col[3] }, ImDrawFlags_None, 2 ds);
		GetWindowDrawList()->AddPolyline(BL, 3, pcolor{ col[0], col[1], col[2], col[3] }, ImDrawFlags_None, 2 ds);
		GetWindowDrawList()->AddPolyline(BR, 3, pcolor{ col[0], col[1], col[2], col[3] }, ImDrawFlags_None, 2 ds);
	}
}

void c_esppreview::draw_bar( c_esp_base_item* item, int bars, int index, ImRect box_bb )
{
	if ( !*item->visible )
		return;

	float thickness = 2 ds;

	ImVec2 target_pos;
	ImVec2 size;
	bool horizontal = false;

	switch ( item->pos )
	{
	case esppos_top: 
		target_pos = box_bb.Min - ImVec2{ 0, thickness + 8 ds + ( thickness + 4 ds ) * bars };
		size = ImVec2{ box_bb.GetWidth( ), thickness };
		horizontal = true;
		break;
	case esppos_bottom: 
		target_pos = ImVec2{ box_bb.Min.x, box_bb.Max.y } + ImVec2{ 0, 8 ds + ( thickness + 4 ds ) * bars };
		size = ImVec2{ box_bb.GetWidth( ), thickness };
		horizontal = true;
		break;
	case esppos_left: 
		target_pos = box_bb.Min - ImVec2{ thickness + 8 ds + ( thickness + 4 ds ) * bars, 0 };
		size = ImVec2{ thickness, box_bb.GetHeight( ) };
		break;
	case esppos_right: 
		target_pos = ImVec2{ box_bb.Max.x, box_bb.Min.y } + ImVec2{ 8 ds + ( thickness + 4 ds ) * bars, 0 };
		size = ImVec2{ thickness, box_bb.GetHeight( ) };
		break;
	}

	ImVec2 preview_pos;
	if ( item->dragging ) {
		preview_pos = target_pos;
		target_pos = GetIO( ).MousePos;
	}

	item->rect.Min = target_pos;
	item->rect.Max = item->rect.Min + size;

	if ( item->posvec2 == ImVec2{ 0, 0 } ) item->posvec2 = target_pos - GetWindowPos( );
	item->posvec2 = ImLerp( item->posvec2, target_pos - GetWindowPos( ), GetIO( ).DeltaTime * 17 );

	ImRect bb{ GetWindowPos( ) + item->posvec2, GetWindowPos( ) + item->posvec2 + size };
	bool hovered = IsMouseHoveringRect( item->rect.Min, item->rect.Max ) && GImGui->HoveredWindow == GetCurrentWindow( );

	GetWindowDrawList( )->AddRectFilled( bb.Min, bb.Max, pcolor{ item->col[0], item->col[1], item->col[2], item->col[3] * 0.15f }, 3 ds );
	GetWindowDrawList( )->AddRectFilled( horizontal ? bb.Min + vec2{ 2, 2 } : ImVec2{ bb.Min.x + 2 ds, bb.Max.y - 2 ds - ( bb.GetHeight( ) - 4 ds ) * 0.5f }, horizontal ? ImVec2{ bb.Min.x + 2 ds + ( bb.GetWidth( ) - 4 ds ) * 0.5f, bb.Max.y - 2 ds } : bb.Max - vec2{ 2, 2 }, pcolor{ item->col[0], item->col[1], item->col[2], item->col[3] }, 3 ds );

	item_popup( item->name.c_str( ), item, item->settings_open, { bb.Max.x + 8 ds, bb.Min.y } );
	if ( hovered && IsMouseClicked( 1 ) ) {
		item->settings_open = true;
	}

	if ( hovered && IsMouseClicked( 0 ) ) {
		item->dragging = true;
	} if ( !IsMouseDown( 0 ) ) {
		item->dragging = false;
	}

	item->just_swapped = false;

	if ( item->dragging )
	{
		GetWindowDrawList( )->AddShadowRect( preview_pos, preview_pos + size, g_style->col( pcol_border, 5 ), 30 ds, { 0, 0 }, 0, 10 ds );

		bool swapped = false;
		for ( int i = 0; i < items.size( ); ++i )
		{
			auto* it = items[i];

			if ( it == item || it->just_swapped ) continue;

			if ( it->type != espitem_bar || !IsMouseHoveringRect( it->rect.Min, it->rect.Max ) ) {
				continue;
			}

			std::swap( items[i], items[index] );
			items[index]->just_swapped = true;
			items[i]->just_swapped = true;

			swapped = true;
			break;
		}

		if ( !swapped )
		{
			for ( auto& area : areas )
			{
				if ( area.pos == item->pos ) {
					continue;
				}

				if ( !IsMouseHoveringRect( area.rect.Min, area.rect.Max ) ) {
					continue;
				}

				item->pos = area.pos;
				break;
			}
		}
	}
}

void c_esppreview::draw_text( c_esp_base_item* item, int bars, int texts, int index, ImRect box_bb )
{
	if ( !*item->visible )
		return;

	ImVec2 target_pos;
	ImVec2 size = CalcTextSize( static_cast< c_esp_text* >( item )->label.c_str( ) );

	switch ( item->pos )
	{
	case esppos_top: 
		target_pos = ImVec2{ box_bb.GetCenter( ).x - size.x / 2, box_bb.Min.y - 8 ds - size.y - ( 10 ds ) * bars - ( size.y + 4 ds ) * texts };
		break;
	case esppos_bottom: 
		target_pos = ImVec2{ box_bb.GetCenter( ).x - size.x / 2, box_bb.Max.y } + ImVec2{ 0, 8 ds + ( 10 ds ) * bars + ( size.y + 4 ds ) * texts };
		break;
	case esppos_left: 
		target_pos = box_bb.Min - ImVec2{ 8 ds + ( 10 ds ) * bars + size.x, -( size.y + 4 ds ) * texts };
		break;
	case esppos_right: 
		target_pos = ImVec2{ box_bb.Max.x, box_bb.Min.y } + ImVec2{ 8 ds + ( 10 ds ) * bars, ( size.y + 4 ds ) * texts };
		break;
	}

	ImVec2 preview_pos;
	if ( item->dragging ) {
		preview_pos = target_pos;
		target_pos = GetIO( ).MousePos;
	}

	item->rect.Min = target_pos;
	item->rect.Max = item->rect.Min + size;

	if ( item->posvec2 == ImVec2{ 0, 0 } ) item->posvec2 = target_pos - GetWindowPos( );

	item->posvec2 = ImLerp( item->posvec2, target_pos - GetWindowPos( ), GetIO( ).DeltaTime * 17 );

	ImRect bb{ GetWindowPos( ) + item->posvec2, GetWindowPos( ) + item->posvec2 + size };
	bool hovered = IsMouseHoveringRect( item->rect.Min, item->rect.Max ) && GImGui->HoveredWindow == GetCurrentWindow( );

	if ( item->dragging ) {
		GetWindowDrawList( )->AddRectFilled( bb.Min - ImVec2{ 6, 2 }, bb.Max + ImVec2{ 6, 2 }, g_style->col( pcol_border, 2.5f ), 10 );
	}
	GetWindowDrawList( )->AddText( bb.Min - ImVec2{ 1, 0 }, pcolor{ 0, 0, 0 }, static_cast< c_esp_text* >( item )->label.c_str( ) );
	GetWindowDrawList( )->AddText( bb.Min - ImVec2{ 1, 0 }, pcolor{ 0, 0, 0 }, static_cast< c_esp_text* >( item )->label.c_str( ) );
	GetWindowDrawList( )->AddText( bb.Min + ImVec2{ 0, 1 }, pcolor{ 0, 0, 0 }, static_cast< c_esp_text* >( item )->label.c_str( ) );
	GetWindowDrawList( )->AddText( bb.Min + ImVec2{ 0, 1 }, pcolor{ 0, 0, 0 }, static_cast< c_esp_text* >( item )->label.c_str( ) );
	GetWindowDrawList( )->AddText( bb.Min, pcolor{ item->col[0], item->col[1], item->col[2], item->col[3] }, static_cast< c_esp_text* >( item )->label.c_str( ) );
	
	item_popup( item->name.c_str( ), item, item->settings_open, { bb.Max.x + 8 ds, bb.Min.y } );
	if ( hovered && IsMouseClicked( 1 ) ) {
		item->settings_open = true;
	}

	if ( hovered && IsMouseClicked( 0 ) ) {
		item->dragging = true;
	} if ( !IsMouseDown( 0 ) ) {
		item->dragging = false;
	}

	item->just_swapped = false;

	if ( item->dragging )
	{
		GetWindowDrawList( )->AddShadowRect( preview_pos, preview_pos + size, g_style->col( pcol_border, 5 ), 30 ds, { 0, 0 }, 0, 10 ds );

		bool swapped = false;
		for ( int i = 0; i < items.size( ); ++i )
		{
			auto* it = items[i];

			if ( it == item || it->just_swapped ) continue;

			if ( it->type != espitem_text || !IsMouseHoveringRect( it->rect.Min, it->rect.Max ) ) {
				continue;
			}

			std::swap( items[i], items[index] );
			items[index]->just_swapped = true;
			items[i]->just_swapped = true;

			swapped = true;
			break;
		}

		if ( !swapped )
		{
			for ( auto& area : areas )
			{
				if ( area.pos == item->pos ) {
					continue;
				}

				if ( !IsMouseHoveringRect( area.rect.Min, area.rect.Max ) ) {
					continue;
				}

				item->pos = area.pos;
				break;
			}
		}
	}
}

void c_esppreview::settings_item( c_esp_base_item* item )
{
	struct s {
		float enabled;
		float hover;
		bool open = false;
	}; auto& obj = anim_obj( item->name.c_str( ), 12736472, s{ } );

	bool pressed = InvisibleButton( item->name.c_str( ), CalcTextSize( item->name.c_str( ) ) + vec2{ 16, 12 } );
	bool hovered = IsItemHovered( );
	ImRect& bb = GImGui->LastItemData.Rect;

	if ( pressed ) {
		*item->visible = !*item->visible;
	}

	char temp[128];
	ImFormatString( temp, sizeof( temp ), "%s settings", item->name.c_str( ) );
	item_popup( temp, item, obj.open, { GImGui->LastItemData.Rect.Max.x + 4 ds, GImGui->LastItemData.Rect.Min.y } );

	if ( IsMouseClicked( 1 ) && hovered ) {
		obj.open = !obj.open;
	}

	obj.hover = anim( obj.hover, 0.f, 1.f, hovered );
	obj.enabled = anim( obj.enabled, 0.f, 1.f, *item->visible );

	GetWindowDrawList( )->AddRectFilled( bb.Min, bb.Max, g_style->col( pcol_bg4, obj.enabled ), 4 ds );
	GetWindowDrawList( )->AddText( bb.Min + vec2{ 8, 6 }, col_anim( col_anim( g_style->col( pcol_text2 ), g_style->col( pcol_text3 ), obj.hover ), g_style->col( pcol_text ), obj.enabled ), item->name.c_str( ) );
}
void c_esppreview::item_popup( const char* str_id, c_esp_base_item* item, bool& open, ImVec2 pos )
{
	struct s {
		float anim;
	}; auto& obj = anim_obj( str_id, 12736472, s{ } );

	obj.anim = anim( obj.anim, 0.f, 1.f, open );

	if ( widgets->popup.begin( str_id, obj.anim, pos, vec2{ 14, 14 } ) )
	{
		PushStyleVar( ImGuiStyleVar_ItemSpacing, vec2{ 12, 12 } );
		PushItemWidth( 168 ds );

		TextDisabled( item->name.c_str( ) );

		SameLine( CalcItemWidth( ) );
		char temp[256];
        memset( temp, 0, sizeof( temp ) );
		ImFormatString( temp, sizeof( temp ), "close %s", str_id );
		if ( widgets->iconbutton( temp, close_medium_filled ) ) {
			open = false;
		}

		if ( ( IsMouseClicked( 1 ) && !IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) ) || ( IsMouseClicked( 0 ) ) && ( !GImGui->HoveredWindow || !strstr( GImGui->HoveredWindow->Name, "popup" ) ) ) {
			open = false;
		}

        memset( temp, 0, sizeof( temp ) );
		ImFormatString( temp, sizeof( temp ), "Color##%s", str_id );
		widgets->coloredit( temp, item->col );
		switch ( item->type )
		{
		case espitem_bar:
		{
            memset( temp, 0, sizeof( temp ) );
			ImFormatString( temp, sizeof( temp ), "Rounded##%s", str_id );
			widgets->checkbox( temp, static_cast< c_esp_bar* >( item )->rounded );
		}
		break;
		case espitem_text:
		{
		
		}
		break;
		case espitem_box:
		{
            memset( temp, 0, sizeof( temp ) );
			ImFormatString( temp, sizeof( temp ), "Type##%s", item->name.c_str( ) );
			widgets->combo( temp, static_cast< c_esp_box* >( item )->type, { "Full", "Cornered" } );
		}
		break;
		}

		PopItemWidth( );
		PopStyleVar( );

		widgets->popup.end( );
	}
}

void c_esppreview::draw( const ImVec2& center, bool* box_enabled, float* box_col, int* box_type, bool* skeleton_enabled, float* skeleton_col, pcolor* chams_col, int* chams_type )
{
	GetCurrentWindow( )->Flags |= ImGuiWindowFlags_NoMove;

	if ( !box ) box = new c_esp_box( espitem_box, "Box", esppos_top, box_enabled, box_col );
	box->type = box_type;

	// Match s_f_y_hooker_03 transparent cutout (266x551).
	constexpr float kPedAspect = 266.f / 551.f;
	const float box_h = 360.f;
	const float box_w = box_h * kPedAspect;
	const ImVec2 box_size{ box_w, box_h };
	ImRect box_bb{ center - box_size / 2.f, center + box_size / 2.f };

	if ( areas.empty( ) ) {
		// top
		areas.push_back( { esppos_top, { GetWindowPos( ), { GetWindowPos( ).x + GetWindowWidth( ), box_bb.Min.y } } } );

		// right
		areas.push_back( { esppos_right, { { center.x, box_bb.Min.y }, { GetWindowPos( ).x + GetWindowWidth( ), box_bb.Max.y } } } );

		// bottom
		areas.push_back( { esppos_bottom, { { GetWindowPos( ).x, box_bb.Max.y }, GetWindowPos( ) + GetWindowSize( ) } } );

		// left
		areas.push_back( { esppos_left, { { GetWindowPos( ).x, box_bb.Min.y }, { center.x, box_bb.Max.y } } } );
	} else {
		areas[0].rect = ImRect{ GetWindowPos( ), { GetWindowPos( ).x + GetWindowWidth( ), box_bb.Min.y } };
		areas[1].rect = ImRect{ { center.x, box_bb.Min.y }, { GetWindowPos( ).x + GetWindowWidth( ), box_bb.Max.y } };
		areas[2].rect = ImRect{ { GetWindowPos( ).x, box_bb.Max.y }, GetWindowPos( ) + GetWindowSize( ) };
		areas[3].rect = ImRect{ { GetWindowPos( ).x, box_bb.Min.y }, { center.x, box_bb.Max.y } };
	}

	draw_ped_preview( box_bb );
	draw_preview_skeleton( box_bb, skeleton_enabled, skeleton_col );
	draw_box( box_bb, box->visible, box->col, box->type );

	int bars[4] { 0, 0, 0, 0 };
	int texts[4] { 0, 0, 0, 0 };

	// bars
	for ( int i = 0; i < items.size( ); ++i )
	{
		auto* item = items[i];

		if ( item->type != espitem_bar || !*item->visible )
			continue;

		draw_bar( static_cast< c_esp_bar* >( item ), bars[item->pos], i, box_bb );

		bars[item->pos]++;
	}

	// texts
	for ( int i = 0; i < items.size( ); ++i )
	{
		auto* item = items[i];

		if ( item->type != espitem_text || !*item->visible )
			continue;

		draw_text( static_cast< c_esp_bar* >( item ), bars[item->pos], texts[item->pos], i, box_bb );
		texts[item->pos]++;
	}
}