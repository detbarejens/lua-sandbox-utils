#pragma once

class c_search {
	std::vector< c_searchitem > items;
public:
	char buf[32];

	void initialize( );
	void add_item( c_searchitem item );
	void draw( );
	int size( ) {
		return items.size( );
	}
};

inline auto g_search = std::make_unique< c_search >( );