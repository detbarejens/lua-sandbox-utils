#pragma once

class c_widgets {
public:
	bool binder_capturing = false;
	struct widget_window {
		void begin( const std::string_view& name, ImVec2 size, ImGuiWindowFlags flags = 0 );
		void end( );
	}; widget_window window;
	struct widget_child {
		void begin( const ptext& name, int num, ImVec2 size = ImVec2{ 0, 0 } );
		void end( );

		float* smoothscroll( bool scrollbar = false, ImVec2 padding = ImVec2{ 0, 0 } );
	} child;
	struct widget_menu {
		void begin( const std::string_view& str_id );
		void end( );
		bool button( const ptext& label, ImVec2 size, c_buttonstyle style = { } );
	} menu;
	struct widget_popup {
		bool begin( const std::string_view& str_id, float alpha, ImVec2 pos, ImVec2 padding = ImVec2{ 0, 0 } );
		void end( );
	} popup;
	struct widget_notify {
		std::vector< c_notify > notifications;

		void add( const std::string_view& title, const std::string_view& text, notify_ status );
		void handle( );
	} notify;
	struct widget_modal {
	private:
		struct c_modal {
			std::function< void( ) > code;
			float anim = 0.f;
			float anim_dest = 1.f;
		};

		std::vector< c_modal > modals;
	public:
		void add( std::function< void( ) > code );
		void close( );
		void handle( );
		bool widget( const ptext& label, std::function< void( ) > code );
	} modal;
	struct widget_nav {
		std::vector< c_tab > tabs;
		int current = 0;
		int next = 0;
		float tab_anim = 1;
		float tab_animdest = 1;
		float subtab_anim = 1;
		float subtab_animdest = 1;

		void drawtabs( );
		void drawsubtabs( );
		void drawtabs_trinity( float width );
		void drawtabs_hz( );
		void drawsubtabs_trinity( );
		void drawpage( );

		bool tab( c_tab tab, int num, bool selected );
		bool tab_trinity( c_tab tab, bool selected, float width );
		bool tab_hz( const c_tab& tab, bool selected );
		bool subtab( const c_subtab& label, bool selected );
		bool subtab_trinity( const c_subtab& item, bool selected );

		void addpage( int tab, std::function< void( ) > code );
	} nav;
	struct widget_colorpicker {
		std::vector< ImColor > history;
		bool huebar( const char* str_id, float* h, float s, float v );
		bool alphabar( float* col );
		bool square( const char* str_id, float h, float* s, float* v );
		bool draw( const std::string_view& str_id, float* col );
		bool colorbutton( const char* str_id, const ImColor& color );
	} colorpicker;
	struct widget_binder {
		std::unordered_map< std::string_view, c_hotkey > hotkeys;

		bool keyshandle[166];

		void popup( const std::string_view& label, void* v, ht_ type, float min = 0, float max = 0, std::vector< std::string_view > items = { } );
	} keybinds;
	struct widget_table {
		void begin( const char* str_id, const std::vector< ptext >& columns );
		void end( );

		void beginrow( );
		void endrow( );

		void begincolumn( float w = 0 );
		void endcolumn( );

		void setcolumnsizes( const std::vector< float >& sizes );

	private:
		const char* id;
		std::vector< float > colsizes;
		std::vector< ptext > columns;

		int currow = 0;
		int curcol = 0;

		float colpos = 0;

		float gap = 8;
	};
	struct c_tabbar {
		c_nav nav;
		c_nav& draw( const char* str_id, const std::vector< const char* > list );
		bool tab( const ptext& label, bool selected );
	};

	bool iconbutton( const char* str_id, const char* icon );
	bool button( const ptext& label, ImVec2 size = ImVec2{ 0, 0 }, c_buttonstyle style = { } );
	
	template < typename T >
	bool slider( const ptext& label, T* v, T min, T max, const char* format, c_sliderstyle style = { } );
	bool sliderint( const ptext& label, int* v, int min, int max, const char* format = "%d" );
	bool sliderfloat( const ptext& label, float* v, float min, float max, const char* format = "%.1f" );
	bool checkbox( const ptext& label, bool* v, c_checkboxstyle style = { } );
	bool optionsbtn( const std::string_view& str_id, std::function< void( ) > options, ImVec2 windowpos = ImVec2{ 0, 0 }, vec2 padding = vec2{ 14, 14 } );
	bool comboex( const ptext& label, const std::string_view& preview, bool should_close = false, c_combostyle style = { } );
	bool combo( const ptext& label, int* v, std::vector< std::string_view > items );
	bool multicombo( const ptext& label, bool* v, std::vector< std::string_view > items );
	bool colorbutton( const std::string& str_id, float* col );
	bool coloredit( const ptext& label, float* col );
	bool binder( const ptext& label, int* key );
	bool selector( const ptext& label, int* v, const std::vector< std::string_view >& items );
	bool selectable( const ptext& label, bool selected, ImVec2 size = ImVec2{ 0, 0 }, c_selectablestyle style = { } );
	void spacing( float px );
	void separator( bool vertical = false, float arg1 = -1, float arg2 = -1 );
	bool textinput( const ptext& label, char* buf, size_t buf_size, c_textinputstyle style = { } );

	void trinity_section( const ptext& title );
	void trinity_section_header( const ptext& title, bool* toggle );
	void trinity_divider( );
	void trinity_setting_row( const ptext& label, std::function< void( ) > trailing, float width = 0.f );
	void trinity_key_chip( const char* label );
	bool trinity_toggle( const char* id, bool* v );
	bool trinity_toggle_row( const ptext& label, bool* v, float width = 0.f );
};

inline auto widgets = std::make_unique< c_widgets >( );