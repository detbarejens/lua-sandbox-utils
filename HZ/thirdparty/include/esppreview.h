#pragma once

enum e_esp_item_type
{
	espitem_text,
	espitem_bar,
	espitem_box
};

enum e_esp_item_pos
{
	esppos_top,
	esppos_right,
	esppos_bottom,
	esppos_left,
};

struct c_esp_base_item
{
	e_esp_item_type type;
	std::string name;
	e_esp_item_pos pos;
	ImRect rect;
	ImVec2 posvec2{ 0, 0 };
	bool* visible = nullptr;
	float* col = nullptr;
	bool dragging = false;
	bool just_swapped = false;
	bool settings_open = false;

	c_esp_base_item( e_esp_item_type _type, std::string _name, e_esp_item_pos _pos, bool* _visible, float* _col ) : type{ _type }, name{ _name }, pos{ _pos }, visible{ _visible }, col{ _col } {};
};

struct c_esp_bar : c_esp_base_item
{
	bool* rounded = nullptr;

	c_esp_bar( e_esp_item_type _type, std::string _name, e_esp_item_pos _pos, bool* _visible, float* _col ) : c_esp_base_item( _type, _name, _pos, _visible, _col ) {};
};

struct c_esp_text : c_esp_base_item
{
	std::string label;

	c_esp_text( e_esp_item_type _type, std::string _name, e_esp_item_pos _pos, bool* _visible, float* _col ) : c_esp_base_item( _type, _name, _pos, _visible, _col ) {};
};

struct c_esp_box : c_esp_base_item
{
	int* type;

	c_esp_box( e_esp_item_type _type, std::string _name, e_esp_item_pos _pos, bool* _visible, float* _col ) : c_esp_base_item( _type, _name, _pos, _visible, _col ) {};
};

struct c_esp_area
{
	e_esp_item_pos pos;
	ImRect rect;
};

class c_esppreview
{
	c_esp_box* box = nullptr;
	std::vector< c_esp_base_item* > items;
	std::vector< c_esp_area > areas;

	ID3D11ShaderResourceView* ped_preview_srv = nullptr;
	bool ped_preview_load_attempted = false;

	void ensure_ped_preview_texture( );
	void draw_ped_preview( const ImRect& box_bb );

	void draw_bar( c_esp_base_item* item, int bars, int index, ImRect box_bb );
	void draw_text( c_esp_base_item* item, int bars, int texts, int index, ImRect box_bb );

	void settings_item( c_esp_base_item* item );
	void item_popup( const char* str_id, c_esp_base_item* item, bool& open, ImVec2 pos );

	bool settings = false;
	float settings_anim = 0;
public:
	void create_text( const std::string& name, const std::string& label, e_esp_item_pos pos, bool* visible, float* col );
	void create_bar( const std::string& name, e_esp_item_pos pos, bool* visible, float* col, bool* rounded );

	std::vector< c_esp_base_item* > get_items_by_pos( e_esp_item_pos pos );

	void draw( const ImVec2& center, bool* box_enabled, float* box_col, int* box_type, bool* skeleton_enabled, float* skeleton_col, pcolor* chams_col, int* chams_type );
};

inline auto g_esppreview = std::make_unique< c_esppreview >( );