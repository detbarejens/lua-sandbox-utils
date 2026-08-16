#pragma once

class c_draw {
public:
	void glow( const std::string& callback_name, float intensity, pcolor col );
	void gradient( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col1, ImColor col2, ImColor col3, ImColor col4, float rounding, ImDrawFlags flags = 0 );
	void gradientoutline( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col1, ImColor col2, ImColor col3, ImColor col4, float rounding, ImDrawFlags flags = 0, float thickness = 1 );
	void rotatestart( );
	ImVec2 rotationcenter( );
    void rotateend( float rad, ImVec2 center );
	void text( int font, float fontsize, ImVec2 pos, pcolor col, std::string_view text, const char* text_end = 0, bool shadow = false, ImDrawList* draw = ImGui::GetWindowDrawList( ) );
	void textitem( int font, float fontsize, pcolor col, std::string_view text, bool shadow = true );
	ImVec2 textsize( int font, float fontsize, const char* text );
};

inline auto g_draw = std::make_unique< c_draw >( );