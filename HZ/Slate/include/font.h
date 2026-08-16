#pragma once

class c_fonts {
public:
	std::vector< c_font > fonts;
};

inline auto g_fonts = std::make_unique< c_fonts >( );
inline auto& fonts = g_fonts->fonts;