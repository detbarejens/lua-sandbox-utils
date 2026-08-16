#pragma once

inline std::vector< const char* > keys {
	"NONE",
	"M1",
	"M2",
	"CN",
	"M3",
	"M4",
	"M5",
	"...",
	"BACK",
	"TAB",
	"NONE",
	"NONE",
	"BACK",
	"ENT",
	"NONE",
	"NONE",
	"SHIFT",
	"CTRL",
	"ALT",
	"PAUSE",
	"CAPS",
	"KAN",
	"NONE",
	"JUN",
	"FIN",
	"KAN",
	"NONE",
	"ESC",
	"CON",
	"NCO",
	"ACC",
	"MAD",
	"SPACE",
	"PGU",
	"PGD",
	"END",
	"HOME",
	"LEFT",
	"UP",
	"RIGH",
	"DOWN",
	"SEL",
	"PRINT",
	"EXE",
	"PRINT",
	"INS",
	"DEL",
	"HEL",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"...",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"A",
	"B",
	"C",
	"D",
	"E",
	"F",
	"G",
	"H",
	"I",
	"J",
	"K",
	"L",
	"M",
	"N",
	"O",
	"P",
	"Q",
	"R",
	"S",
	"T",
	"U",
	"V",
	"W",
	"X",
	"Y",
	"Z",
	"WIN",
	"WIN",
	"APP",
	"NONE",
	"SLE",
	"NUM0",
	"NUM1",
	"NUM2",
	"NUM3",
	"NUM4",
	"NUM5",
	"NUM6",
	"NUM7",
	"NUM8",
	"NUM9",
	"*",
	"+",
	"|",
	"-",
	".",
	"/",
	"F1",
	"F2",
	"F3",
	"F4",
	"F5",
	"F6",
	"F7",
	"F8",
	"F9",
	"F10",
	"F11",
	"F12",
	"F13",
	"F14",
	"F15",
	"F16",
	"F17",
	"F18",
	"F19",
	"F20",
	"F21",
	"F22",
	"F23",
	"F24",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NUMLOCK",
	"SCR",
	"=",
	"MAS",
	"TOY",
	"OYA",
	"OYA",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"NONE",
	"SHIFT",
	"SHIFT",
	"CTRL",
	"CTRL",
	"ALT",
	"ALT"
};

struct vec2 {
	float x, y;

	vec2( float _x, float _y ) {
        x = _x;
        y = _y;
    }
	vec2( ImVec2 s ) {
        x = s.x;
        y = s.y;
    }

	operator ImVec2( ) const;
};

enum pcol_ {
	pcol_scheme,
	pcol_text,
	pcol_text2,
	pcol_text3,
	pcol_bg,
	pcol_bg2,
	pcol_bg3,
	pcol_bg4,
	pcol_bg5,
	pcol_bg6,
	pcol_bg7,
	pcol_border,
	pcol_separator,
	pcol_checkboxdot,
	pcol_textonscheme,
	pcol_listbox,
	pcol_tab,
	pcol_backdrop,
	pcol_count
};

struct pcolor {
	float r, g, b, a;

	operator ImColor( ) const {
		return ImColor{ r, g, b, a * GImGui->Style.Alpha };
	}

	operator ImU32( ) const {
		return ImColor{ r, g, b, a * GImGui->Style.Alpha };
	}

	operator ImVec4( ) const {
		return ImVec4{ r, g, b, a * GImGui->Style.Alpha };
	}

	pcolor lighten( float v ) {
		return pcolor{ r * v, g * v, b * v, a };
	}

	pcolor( float _r, float _g, float _b, float _a ) {
		r = _r;
		g = _g;
		b = _b;
		a = _a;
	}

	pcolor( int _r, int _g, int _b, float _a ) {
		r = _r / 255.f;
		g = _g / 255.f;
		b = _b / 255.f;
		a = _a;
	}

	pcolor( float _r, float _g, float _b ) {
		r = _r / 255.f;
		g = _g / 255.f;
		b = _b / 255.f;
		a = 1.f;
	}

	pcolor( const ImVec4& col ) {
		r = col.x;
		g = col.y;
		b = col.z;
		a = col.w;
	}

	pcolor( const ImU32& col ) {
		r = ImColor{ col }.Value.x;
		g = ImColor{ col }.Value.y;
		b = ImColor{ col }.Value.z;
		a = ImColor{ col }.Value.w;
	}
	
	pcolor( const ImColor& col ) {
		r = col.Value.x;
		g = col.Value.y;
		b = col.Value.z;
		a = col.Value.w;
	}

	pcolor( ) : r( 0 ), g( 0 ), b( 0 ), a( 0 ) {}

	float* f4( ) {
		return &r;
	}

	pcolor h( float newh ) {
		float _h, s, v;
		ImGui::ColorConvertRGBtoHSV( r, g, b, _h, s, v );
		float newr, newg, newb;
		ImGui::ColorConvertHSVtoRGB( newh, s, v, newr, newg, newb );
		return pcolor{ newr, newg, newb, a };
	}

	float geth( ) {
		float h, s, v;
		ImGui::ColorConvertRGBtoHSV( r, g, b, h, s, v );
		return h;
	}

	pcolor alpha( float newa ) {
		return pcolor{ r, g, b, newa * GImGui->Style.Alpha * a };
	}

	ImVec4 vec4( ) {
		return ImVec4{ r, g, b, a * GImGui->Style.Alpha };
	}

	operator bool( ) const {
		return a > 0.02f;
	}

	bool operator==( const pcolor& other ) {
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	bool operator!=( const pcolor& other ) {
		return r != other.r || g != other.g || b != other.b || a != other.a;
	}
};

struct c_image {
private:
	ID3D11ShaderResourceView* image;
public:
	float w, h;

	void load( ID3D11Device* device, void* data, size_t data_size ) {
		D3DX11_IMAGE_LOAD_INFO image_load_info;
		D3DX11CreateShaderResourceViewFromMemory( device, data, data_size, &image_load_info, 0, &image, 0 );
		w = image_load_info.Width;
		h = image_load_info.Height;
	}

	void load( ID3D11Device* device, const std::string_view& url ) {
		if ( image != nullptr )
			return;

		std::unique_ptr< std::remove_pointer_t< HINTERNET >, decltype( &InternetCloseHandle ) >
			hInternet(InternetOpenA("ImGui ImageLoader", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0), InternetCloseHandle);

		if (!hInternet) {
			throw std::runtime_error("InternetOpenA failed");
		}

		std::unique_ptr< std::remove_pointer_t< HINTERNET >, decltype( &InternetCloseHandle ) >
			hURL( InternetOpenUrlA( hInternet.get( ), url.data( ), NULL, 0, INTERNET_FLAG_RELOAD, 0 ), InternetCloseHandle );

		if ( !hURL ) {
			throw std::runtime_error( "InternetOpenUrl failed" );
		}

		std::vector<BYTE> image_data;
		std::vector<BYTE> buffer( 4096 );
		DWORD bytes_read = 0;
		while (InternetReadFile(hURL.get(), buffer.data(), 4096, &bytes_read) && bytes_read > 0) {
			image_data.insert(image_data.end(), buffer.data(), buffer.data() + bytes_read);
		}

		D3DX11_IMAGE_LOAD_INFO image_load_info;
		ID3DX11ThreadPump* thread_pump{ nullptr };
		HRESULT hr = D3DX11CreateShaderResourceViewFromMemory( device, image_data.data( ), image_data.size( ), &image_load_info, thread_pump, &image, 0 );
		if ( FAILED( hr ) ) {
			while ( !FAILED( hr ) ) {
				hr = D3DX11CreateShaderResourceViewFromMemory( device, image_data.data( ), image_data.size( ), &image_load_info, thread_pump, &image, 0 );
			}
		}
	}

	operator ImTextureID( ) const {
		return ( ImTextureID )image;
	}
};

struct c_font {
	struct font {
		float sz; 
		ImFont* font;	
	};

	std::vector< font > fonts;

	void setup( unsigned char* data, size_t data_size, std::vector< float > sizes, const ImWchar* ranges );

	ImFont* get( float size ) {
		auto it = std::find_if( fonts.begin( ), fonts.end( ),  [&]( const font& a ) {
			return a.sz == size;
		} );

		if ( it == fonts.end( ) ) {
			return nullptr;
		}

		return it->font;
	}
};

struct c_nav {
	int cur;
	int next;
	float anim = 0.f;
	float animdest = 1.f;

	void handle( ) {
		anim = ImLerp( anim, animdest, ImGui::GetIO( ).DeltaTime * 17 );

		if ( anim < 0.05f ) {
			animdest = 1.f;
			cur = next;
		}
	}
};

enum fonts_ {
   ttsupermolotneuetrl_db,
   ttsupermolotneuetrl_md,
   icons,
    fonts_size
};

enum button_ {
	button_default,
	button_outline,
};
enum align_ {
	align_left,
	align_center,
	align_right
};
struct c_buttonstyle {
	button_ style = button_default;
	const char* icon = nullptr;
	float iconsize = 16;
	align_ iconpos = align_right;
	float rounding = 3;
	bool loading = false;
};

struct c_childstyle {
	vec2 padding{ 20, 20 };
	vec2 itemspacing{ 14, 14 };
	bool header = true;
	pcolor borders[4];
	ImGuiWindowFlags flags;
	bool scrollbar = false;
};

enum slider_ {
	slider_horizontal,
	slider_vertical
};
struct c_sliderstyle {
	slider_ style = slider_horizontal;
};

struct c_combostyle {
	float height = 24;
	float xpad = 7;
	float rounding = 2;
	bool rotate = true;
};

struct c_selectablestyle {
	float xpad = 12;
	float rounding = 0;
	bool checkmark = true;
	const char* icon = 0;
};

struct ptext {
	std::string_view str;

	ptext( const std::string_view& s ) : str( s ) { }
	ptext( const char* s ) : str( s ) { }
	ptext( ) : str( "" ) { }

	std::string_view translate( ) const {
		return g_lang->translate( str );
	}

	std::string trim( float maxw ) {
		std::string result;

		for ( int i = 0; i < str.size( ); ++i ) {
			if ( ImGui::CalcTextSize( std::string( result + str[i] ).c_str( ) ).x > maxw ) {
				result.resize( result.size( ) - 3 );
				result += "...";
				break;
			}

			result += str[i];
		}

		return result;
	}
};

struct c_checkboxstyle {
	float square_sz = 16;
	align_ align = align_right;
	bool label = true;
	const char* tooltipicon = 0;
	ptext tooltiptext = "";
	pcolor tooltipcolor = 0;
	std::function< void( ) > options = 0;
	std::vector< float* > col = {};
	int* key = 0;
	bool warning = false;
};

struct c_theme {
	std::vector< pcolor > colors;
};

enum notify_ {
	notify_success,
	notify_warning,
	notify_error,
	notify_info
};

struct c_notify {
	std::string_view title;
	std::string_view message;
	notify_ status;
	float duration = 3.f;
	float time = 0.f;
	ImVec2 pos{ 0, 0 };
	float fade_time = 0.25f;
};

struct c_subtab {
	const char* icon;
	ptext label;
};
struct c_tab {
	const char* icon;
	ptext label;
	std::vector< c_subtab > subtabs{ };

	int current = 0;
	int next = 0;

	std::vector< std::function< void( ) > > pages{ };
	const char* subtitle = nullptr;
};

struct c_tabcategory {
	ptext label;
	int pos;
};

struct c_textinputstyle {
	vec2 padding{ 11, 11 };
	const char* icon = 0;
	std::string_view hint = "";
	ImVec2 size{ 0, 0 };
	pcolor bgcol{ };
	float rounding = 3;
	ImGuiItemFlags flags = 0;
};

struct c_searchitem {
	std::string_view label;
	std::function< void( ) > code;
	c_tab tab;
};

struct c_config {
	std::string_view id;
	const char* name;
	const char* author;
	const char* created;
	const char* modified;
	int users;
};

struct c_script {
	const char* id;
	const char* name;
	const char* author;
	const char* modified;
	bool loaded = false;
};

struct c_pushcol {
	pcol_ col;
	pcolor oldcol;
};

enum ht_ {
	ht_checkbox,
	ht_sliderint,
	ht_sliderfloat,
	ht_combo,
	ht_button,
};

struct c_bind {
	int key;
	int mode;
};

struct c_hotkey {
	std::vector< c_bind > binds;

	ht_ type;
	void* v;
	std::any value;
	std::any saved;
	int selected = 0;
	bool is_active = false;
};

struct c_player {
	std::string_view name;
	bool pinned = false;
	bool eye = false;
	bool refresh = false;
};

struct c_skin {
	int weaponid;
	int genid;
	std::string_view name;
	std::string_view skin;
	std::string_view rarity;
	std::string_view url;
	c_image image;

	c_skin* selected = nullptr;
};

struct c_weapon {
	int weaponid;
	int genid;
};

struct glowuniforms {
	ImVec2 start;
    ImVec2 glowCenter;
    float glowRadius;
    float glowIntensity;
    float alpha;
    float padding1;
	float r, g, b;
    float pad;
};

struct c_codesearchresult {
	int line;
	int column;
};

enum radarobj_type {
	ro_enemy,
	ro_friend,
	ro_npc,
};

struct c_radarobj {
	const char* name;
	radarobj_type type;
	ImVec2 pos;
};