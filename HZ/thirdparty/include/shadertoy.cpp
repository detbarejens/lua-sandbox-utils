#include <slate.h>

cshader glowshader{ R"(
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};
sampler sampler0; 
Texture2D texture0;

cbuffer TimeBuffer : register(b0)
{
    float2 glow_pos;
    float glow_strength;
};

float4 main(PS_INPUT input) : SV_Target
{
    float2 pos = input.pos.xy;
    float4 color = input.col;

    float dist = distance(pos, glow_pos);

    float radius = glow_strength;      // радиус блюра
    float softness = 1.0;              // можно вынести в cbuffer

    float alpha = exp(-pow(dist / radius, 2.0) * softness);

    return float4(color.rgb, color.a * alpha);
};
)" };

cshader shader{ R"(
struct PS_INPUT
{
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv : TEXCOORD0;
};
sampler sampler0;
Texture2D texture0;

cbuffer TimeBuffer : register(b0)
{
    float iTime;
    float2 position;
    float pad;
};

static const float s3 = 1.7320508075688772;
static const float i3 = 0.5773502691896258;

static const float2x2 tri2cart = float2x2(1.0, -0.5, 0.0, 0.5 * s3);
static const float2x2 cart2tri = float2x2(1.0, i3, 0.0, 2.0 * i3);

#define HASHSCALE1 .1031
#define HASHSCALE3 float3(443.897, 441.423, 437.195)

float hash12(float2 p)
{
    float3 p3 = frac(float3(p.xyx) * HASHSCALE1);
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.x + p3.y) * p3.z);
}

float2 hash23(float3 p3)
{
    p3 = frac(p3 * HASHSCALE3);
    p3 += dot(p3, p3.yzx + 19.19);
    return frac((p3.xx + p3.yz) * p3.zy);
}

float3 pal(float t)
{
    const float3 a = float3(0.5, 0.5, 0.5);
    const float3 b = float3(0.5, 0.5, 0.5);
    const float3 c = float3(0.8, 0.8, 0.5);
    const float3 d = float3(0.0, 0.2, 0.5);
    float3 col = saturate(a + b * cos(6.28318 * (c * t + d)));
    float gray = dot(col, float3(0.299, 0.587, 0.114));
    return float3(gray, gray, gray);
}

float3 bary(float2 v0, float2 v1, float2 v2)
{
    float inv_denom = 1.0 / (v0.x * v1.y - v1.x * v0.y);
    float v = (v2.x * v1.y - v1.x * v2.y) * inv_denom;
    float w = (v0.x * v2.y - v2.x * v0.y) * inv_denom;
    float u = 1.0 - v - w;
    return float3(u, v, w);
}

float dseg(float2 xa, float2 ba)
{
    return length(xa - ba * clamp(dot(xa, ba) / dot(ba, ba), 0.0, 1.0));
}

float2 randCircle(float3 p)
{
    float2 rt = hash23(p);
    float r = sqrt(rt.x);
    float theta = 6.283185307179586 * rt.y;
    return r * float2(cos(theta), sin(theta));
}

float2 randCircleSpline(float2 p, float t)
{
    float t1 = floor(t);
    t -= t1;

    float2 pa = randCircle(float3(p, t1 - 1.0));
    float2 p0 = randCircle(float3(p, t1));
    float2 p1 = randCircle(float3(p, t1 + 1.0));
    float2 pb = randCircle(float3(p, t1 + 2.0));

    float2 m0 = 0.5 * (p1 - pa);
    float2 m1 = 0.5 * (pb - p0);

    float2 c3 = 2.0 * p0 - 2.0 * p1 + m0 + m1;
    float2 c2 = -3.0 * p0 + 3.0 * p1 - 2.0 * m0 - m1;
    float2 c1 = m0;
    float2 c0 = p0;

    return (((c3 * t + c2) * t + c1) * t + c0) * 0.8;
}

float2 triPoint(float2 p, float time)
{
    float t0 = hash12(p);
    return mul(p, tri2cart) + 0.45 * randCircleSpline(p, 0.15 * time + t0);
}

void tri_color(float2 p,
               float4 t0, float4 t1, float4 t2,
               float scl, float time,
               inout float4 cw)
{
    float2 p0 = p - t0.xy;
    float2 p10 = t1.xy - t0.xy;
    float2 p20 = t2.xy - t0.xy;

    float3 b = bary(p10, p20, p0);

    float d10 = dseg(p0, p10);
    float d20 = dseg(p0, p20);
    float d21 = dseg(p - t1.xy, t2.xy - t1.xy);

    float d = min(min(d10, d20), d21);
    d *= -sign(min(b.x, min(b.y, b.z)));

    if (d < 0.5 * scl)
    {
        float2 tsum = t0.zw + t1.zw + t2.zw;

        float3 h_tri = float3(hash12(tsum + t0.zw),
                              hash12(tsum + t1.zw),
                              hash12(tsum + t2.zw));

        float2 pctr = (t0.xy + t1.xy + t2.xy) / 3.0;

        float theta = 1.0 + 0.01 * time;
        float2 dir = float2(cos(theta), sin(theta));

        float grad_input = dot(pctr, dir) - sin(0.05 * time);
        float h0 = sin(0.7 * grad_input) * 0.5 + 0.5;

        h_tri = lerp(float3(h0, h0, h0), h_tri, 0.4);

        float h = dot(h_tri, b);
        float3 c = pal(h);

        float w = smoothstep(0.5 * scl, -0.5 * scl, d);
        cw += float4(w * c, w);
    }
}

float4 main(PS_INPUT input) : SV_Target
{
    float2 iResolution = float2(684.0, 424.0);
    float scl = 4.1 / iResolution.y;

    float2 p = (input.pos.xy - position.xy - 0.5 - 0.5 * iResolution) * scl;

    float2 tfloor = floor(mul(p, cart2tri) + 0.5);

    float2 pts[9];
    [unroll]
    for (int i = 0; i < 3; ++i)
    {
        [unroll]
        for (int j = 0; j < 3; ++j)
        {
            pts[3 * i + j] = triPoint(tfloor + float2(i - 1, j - 1), iTime * 3);
        }
    }

    float4 cw = float4(0, 0, 0, 0);

    [unroll]
    for (int ii = 0; ii < 2; ++ii)
    {
        [unroll]
        for (int jj = 0; jj < 2; ++jj)
        {
            float4 t00 = float4(pts[3 * ii + jj],     tfloor + float2(ii - 1, jj - 1));
            float4 t10 = float4(pts[3 * ii + jj + 3], tfloor + float2(ii,     jj - 1));
            float4 t01 = float4(pts[3 * ii + jj + 1], tfloor + float2(ii - 1, jj));
            float4 t11 = float4(pts[3 * ii + jj + 4], tfloor + float2(ii,     jj));

            tri_color(p, t00, t10, t11, scl, iTime * 3, cw);
            tri_color(p, t00, t11, t01, scl, iTime * 3, cw);
        }
    }
    
    return float4( cw.rgb / cw.w, input.col.w );
}
)" };

void c_shadertoy::_pass( c_buffer* buffer )
{
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_ctx->Map( constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
    memcpy( mapped.pData, buffer, sizeof( *buffer ) );
    m_ctx->Unmap( constant_buffer, 0 );

    m_ctx->PSSetConstantBuffers( 0, 1, &constant_buffer );
    m_ctx->PSSetShader( glowshader.shader, 0, 0 );
}
void c_shadertoy::pass( const ImDrawList* drawlist, const ImDrawCmd* cmd )
{
	g_shadertoy->_pass( ( c_buffer* )( cmd->UserCallbackData ) );
}

void c_shadertoy::_pass2( ImVec2 pos )
{
    c_buffer2 buffer = { };
    buffer.iTime = ImGui::GetTime( );
    buffer.pos = pos;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_ctx->Map( constant_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped );
    memcpy( mapped.pData, &buffer, sizeof( buffer ) );
    m_ctx->Unmap( constant_buffer, 0 );

    m_ctx->PSSetConstantBuffers( 0, 1, &constant_buffer );
    m_ctx->PSSetShader( shader.shader, 0, 0 );
}
void c_shadertoy::pass2( const ImDrawList* drawlist, const ImDrawCmd* cmd )
{
    auto* window = ( ImGuiWindow* )( cmd->UserCallbackData );
	g_shadertoy->_pass2( window->Pos );
}


void c_shadertoy::setup( ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swapchain )
{
	m_device = device;
	m_ctx = ctx;
	m_swapchain = swapchain;

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof( c_buffer );
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if ( !constant_buffer )
        device->CreateBuffer( &cbDesc, nullptr, &constant_buffer );

    glowshader.initialize( m_device );
    shader.initialize( m_device );
}
void c_shadertoy::draw( ImDrawList* draw, ImVec2 start, ImVec2 endpos, ImColor col, float rounding, c_buffer* buffer, int flags )
{
    draw->AddCallback( pass, buffer );
    draw->AddRectFilled( start, endpos, col, rounding, flags );
	draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_shadertoy::bg( ImDrawList* draw, ImVec2 start, ImVec2 endpos, ImColor col, float rounding )
{
    draw->AddCallback( pass2, ImGui::GetCurrentWindow( ) );
    draw->AddRectFilled( start, endpos, col, rounding );
	draw->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}