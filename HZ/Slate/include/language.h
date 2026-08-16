#pragma once

class c_language {
    struct c_str {
        std::string en;
        std::string ru;
    };
	struct c_lang {
        std::string name;
        std::vector< c_str > dict{ };
    };
public:
	int selected;
	std::vector< c_lang > list;

	void add_language( std::string name, std::vector< c_str > dict ) {
        list.push_back( c_lang{ name, dict } );
    }

    void initialize( ) {
        add_language( "English", {} );
        add_language( "Russian", {
            { "Enable", reinterpret_cast< const char* >( u8"¬ключить" ) },    
        } );
    }

    std::string_view translate( std::string_view str ) {
        if ( !str.data( ) ) return "";
        if ( selected == 0 )
            return str;

        auto it = std::find_if( list[selected].dict.begin( ), list[selected].dict.end( ), [&]( const c_str& s ) { return strcmp( s.en.data( ), str.data( ) ) == 0; } );
        if ( it == list[selected].dict.end( ) ) return str;

        return it->ru.c_str( );
    }

    std::vector< std::string_view > vec( ) {
        std::vector< std::string_view > result;

        for ( const auto& lang : list ) {
            result.push_back( lang.name.c_str( ) );
        }

        return result;
    }
};

inline auto g_lang = std::make_unique< c_language >( );