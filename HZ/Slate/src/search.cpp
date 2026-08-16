#include "../include/slate.h"

using namespace ImGui;

void c_search::initialize( ) {
	static bool init = false;

	if ( init )
		return;

	for ( int i = 0; i < widgets->nav.tabs.size( ); ++i ) {
		for ( int t = 0; t < widgets->nav.tabs[i].pages.size( ); ++t ) {
			widgets->nav.next = widgets->nav.current = i;
			widgets->nav.tabs[i].next = widgets->nav.tabs[i].current = t;
			widgets->nav.tabs[i].pages[t]( );
			widgets->nav.tabs[i].next = widgets->nav.tabs[i].current = 0;
		}
	}
	widgets->nav.next = widgets->nav.current = 0;

	init = true;
}

void c_search::add_item( c_searchitem item ) {
	if ( std::find_if( items.begin( ), items.end( ), [item]( const c_searchitem& i ) { return i.label == item.label; } ) != items.end( ) )
		return;

	if ( GImGui->CurrentItemFlags & ImGuiItemFlags_NoNav )
		return;

	item.tab = widgets->nav.tabs[widgets->nav.current];

	items.push_back( item );
}

std::string to_lower( const char* str ) {
	std::string result;

	for ( int i = 0; str[i]; ++i ) {
		result += std::tolower( str[i] );
	}

	return result;
}

bool compare( const char* str, const char* substr ) {
	return strstr( to_lower( str ).c_str( ), to_lower( substr ).c_str( ) );
}

void c_search::draw( ) {
	std::vector< c_searchitem* > res;
	for ( c_searchitem& item : items ) {
		if ( compare( item.label.data( ), buf ) ) {
			res.push_back( &item );
		}
	}

	PushItemWidth( GetWindowWidth( ) - GImGui->Style.WindowPadding.x * 2 );
	for ( auto item : res ) {
		if ( item->tab.subtabs.empty( ) ) {
			Text( item->tab.label.translate( ).data( ) );
		} else {
			TextDisabled( "%s, ", item->tab.label.translate( ).data( ) );
			SameLine( 0, 0 );
			Text( item->tab.subtabs[item->tab.current].label.translate( ).data( ) );
		}

		item->code( );

		widgets->separator( );
	}
	PopItemWidth( );
}