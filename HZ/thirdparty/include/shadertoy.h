#pragma once

struct cshader {
    std::string code;
    ID3D11PixelShader* shader;

    cshader( std::string _code ) {
        code = _code;
    }

    void initialize( ID3D11Device* device ) {
        if ( shader ) return;

        ID3DBlob* shader_blob = nullptr;
        ID3DBlob* error_blob = nullptr;
	    HRESULT hr = D3DCompile(
            code.c_str( ),
            strlen( code.c_str( ) ),
            nullptr,
            nullptr,
            nullptr,
            "main",  
            "ps_5_0",
            0, 0,
            &shader_blob,
            &error_blob
        );

        if ( FAILED( hr ) ) {
            MessageBoxA( 0, ( LPCSTR )error_blob->GetBufferPointer( ), ( LPCSTR )"0", 0 );
        }

        device->CreatePixelShader( shader_blob->GetBufferPointer( ), shader_blob->GetBufferSize( ), nullptr, &shader );

        shader_blob->Release( );
        if ( error_blob ) error_blob->Release( );
    }
};

struct ctexture {
	ID3D11Texture2D* texture;
	ID3D11RenderTargetView* rtv;
	ID3D11ShaderResourceView* srv;
    ImVec2 size;

	void init( ID3D11Device* device, ID3D11DeviceContext* ctx, float width = -1, float height = -1 ) {
		if ( texture ) {
            return;
        }

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width == -1 ? ImGui::GetIO( ).DisplaySize.x : width;
        desc.Height = height == -1 ? ImGui::GetIO( ).DisplaySize.y : height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;

        size = ImVec2{ desc.Width * 1.f, desc.Height * 1.f };

        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture);
        if (FAILED(hr)) {
            texture = nullptr;
            return;
        }

        hr = device->CreateRenderTargetView(texture, nullptr, &rtv);
        if (FAILED(hr)) {
            texture->Release();
            texture = nullptr;
            rtv = nullptr;
            return;
        }

		float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };
		ctx->ClearRenderTargetView(rtv, clearColor);

        hr = device->CreateShaderResourceView(texture, nullptr, &srv);
        if (FAILED(hr)) {
            rtv->Release();
            texture->Release();
            rtv = nullptr;
            texture = nullptr;
            srv = nullptr;
            return;
        }
	}
};

struct c_buffer {
	ImVec2 glow_pos;
    float glow_strength;
	float pad;
};

struct c_buffer2 {
	float iTime;
	ImVec2 pos;
    float pad;
};

class c_shadertoy {
	ID3D11Device* m_device;
	ID3D11DeviceContext* m_ctx;
	IDXGISwapChain* m_swapchain;

	ID3D11Buffer* constant_buffer;

	static void begin( const ImDrawList* drawlist, const ImDrawCmd* cmd );
	void _begin( );
	void _pass( c_buffer* buffer );
	static void pass( const ImDrawList* drawlist, const ImDrawCmd* cmd );
    void _pass2( ImVec2 pos );
	static void pass2( const ImDrawList* drawlist, const ImDrawCmd* cmd );
	void _end();
	static void end( const ImDrawList* drawlist, const ImDrawCmd* cmd );
public:
	void setup( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain );
	void draw( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col, float rounding, c_buffer* buffer, int flags = 0 );
	void bg( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col, float rounding );
};

inline auto g_shadertoy = std::make_unique< c_shadertoy >( );