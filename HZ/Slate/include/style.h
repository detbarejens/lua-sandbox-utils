#pragma once

class c_style {
	std::vector< c_pushcol > pushcache;
public:
	float dpi_scale = 1.f;
	bool just_scaled = false;
	int theme = 0;

	std::vector< c_theme > themes;
	std::vector< pcolor > colors;
	pcolor& col( pcol_ c ) { return colors[c]; }
	pcolor col( pcol_ c, float alpha ) { return colors[c].alpha( alpha ); }

	void push( pcol_ col, pcolor newcol );
	void pop( int k = 1 );

	void setup( );
};

inline auto g_style = std::make_unique< c_style >( );