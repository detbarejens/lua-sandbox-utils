#include "../include/slate.h"

using namespace ImGui;

void c_draw::glow( const std::string& callback_name, float intensity, pcolor col ) {

}

void c_draw::gradientoutline( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col1, ImColor col2, ImColor col3, ImColor col4, float rounding, ImDrawFlags flags, float thickness ) {
    if (rounding > 0.0f)
    {
        const int size_before = draw->VtxBuffer.Size;
        draw->AddRect( start, end, IM_COL32_WHITE, rounding, flags, thickness);
        const int size_after = draw->VtxBuffer.Size;

        for (int i = size_before; i < size_after; i++)
        {
            ImDrawVert* vert = draw->VtxBuffer.Data + i;

            ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4( col1 );
            ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4( col4 );
            ImVec4 up_right = ImGui::ColorConvertU32ToFloat4( col2 );
            ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4( col3 );

            float X = ImClamp((vert->pos.x - start.x) / (end.x - start.x), 0.0f, 1.0f);

            // 4 colors - 8 deltas

            float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
            float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

            float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
            float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

            float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
            float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

            float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
            float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


            float Y = ImClamp((vert->pos.y - start.y) / (end.y - start.y), 0.0f, 1.0f);
            float r = r1 + (r2 - r1) * Y;
            float g = g1 + (g2 - g1) * Y;
            float b = b1 + (b2 - b1) * Y;
            float a = a1 + (a2 - a1) * Y;
            ImVec4 RGBA(r, g, b, a);

            RGBA.x = RGBA.x * ImGui::ColorConvertU32ToFloat4(vert->col).x;
            RGBA.y = RGBA.y * ImGui::ColorConvertU32ToFloat4(vert->col).y;
            RGBA.z = RGBA.z * ImGui::ColorConvertU32ToFloat4(vert->col).z;
            RGBA.w = RGBA.w * ImGui::ColorConvertU32ToFloat4(vert->col).w;

            vert->col = ImColor(RGBA);
        }
        return;
    }

    const ImVec2 uv = draw->_Data->TexUvWhitePixel;
    draw->PrimReserve( 6, 4 );
    draw->PrimWriteIdx( (ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 1)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2));
    draw->PrimWriteIdx( (ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 3));
    draw->PrimWriteVtx( start, uv, col1 );
    draw->PrimWriteVtx(ImVec2(end.x, start.y), uv, col2);
    draw->PrimWriteVtx(end, uv, col3);
    draw->PrimWriteVtx(ImVec2(start.x, end.y), uv, col4);
}

void c_draw::gradient( ImDrawList* draw, ImVec2 start, ImVec2 end, ImColor col1, ImColor col2, ImColor col3, ImColor col4, float rounding, ImDrawFlags flags ) {
    if (rounding > 0.0f)
    {
        const int size_before = draw->VtxBuffer.Size;
        draw->AddRectFilled( start, end, IM_COL32_WHITE, rounding, flags);
        const int size_after = draw->VtxBuffer.Size;

        for (int i = size_before; i < size_after; i++)
        {
            ImDrawVert* vert = draw->VtxBuffer.Data + i;

            ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4( col1 );
            ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4( col4 );
            ImVec4 up_right = ImGui::ColorConvertU32ToFloat4( col2 );
            ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4( col3 );

            float X = ImClamp((vert->pos.x - start.x) / (end.x - start.x), 0.0f, 1.0f);

            // 4 colors - 8 deltas

            float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
            float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

            float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
            float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

            float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
            float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

            float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
            float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


            float Y = ImClamp((vert->pos.y - start.y) / (end.y - start.y), 0.0f, 1.0f);
            float r = r1 + (r2 - r1) * Y;
            float g = g1 + (g2 - g1) * Y;
            float b = b1 + (b2 - b1) * Y;
            float a = a1 + (a2 - a1) * Y;
            ImVec4 RGBA(r, g, b, a);

            RGBA.x = RGBA.x * ImGui::ColorConvertU32ToFloat4(vert->col).x;
            RGBA.y = RGBA.y * ImGui::ColorConvertU32ToFloat4(vert->col).y;
            RGBA.z = RGBA.z * ImGui::ColorConvertU32ToFloat4(vert->col).z;
            RGBA.w = RGBA.w * ImGui::ColorConvertU32ToFloat4(vert->col).w;

            vert->col = ImColor(RGBA);
        }
        return;
    }

    const ImVec2 uv = draw->_Data->TexUvWhitePixel;
    draw->PrimReserve( 6, 4 );
    draw->PrimWriteIdx( (ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 1)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2));
    draw->PrimWriteIdx( (ImDrawIdx)(draw->_VtxCurrentIdx)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 2)); draw->PrimWriteIdx((ImDrawIdx)(draw->_VtxCurrentIdx + 3));
    draw->PrimWriteVtx( start, uv, col1 );
    draw->PrimWriteVtx(ImVec2(end.x, start.y), uv, col2);
    draw->PrimWriteVtx(end, uv, col3);
    draw->PrimWriteVtx(ImVec2(start.x, end.y), uv, col4);
}

int rotation_start_index;
void c_draw::rotatestart( )
{
    rotation_start_index = GetWindowDrawList( )->VtxBuffer.Size;
}

ImVec2 c_draw::rotationcenter( )
{
    ImVec2 l( FLT_MAX, FLT_MAX ), u( -FLT_MAX, -FLT_MAX );

    const auto& buf = GetWindowDrawList( )->VtxBuffer;
    for ( int i = rotation_start_index; i < buf.Size; i++ )
        l = ImMin( l, buf[i].pos ), u = ImMax( u, buf[i].pos );

    return ImVec2( ( l.x + u.x ) / 2, ( l.y + u.y ) / 2 );
}

void c_draw::rotateend( float rad, ImVec2 center )
{
    float s = sin( rad ), c = cos( rad );
    center = ImRotate( center, s, c) - center;

    auto& buf = GetWindowDrawList()->VtxBuffer;
    for ( int i = rotation_start_index; i < buf.Size; i++ )
        buf[i].pos = ImRotate( buf[i].pos, s, c ) - center;
}

void c_draw::text( int font, float fontsize, ImVec2 pos, pcolor col, std::string_view text, const char* text_end, bool shadow, ImDrawList* draw ) {
    ImFont* loaded = fonts[font].get( fontsize );
    if ( !loaded )
        loaded = ImGui::GetFont( );
    if ( !loaded || !draw )
        return;

    if ( shadow ) {
        draw->AddText( loaded, loaded->FontSize, pos - ImVec2{ 1, 0 }, pcolor{ 0, 0, 0, 0.5f }, text.data( ), FindRenderedTextEnd( text.data( ) ) );
        draw->AddText( loaded, loaded->FontSize, pos - ImVec2{ 0, 1 }, pcolor{ 0, 0, 0, 0.5f }, text.data( ), FindRenderedTextEnd( text.data( ) ) );
        draw->AddText( loaded, loaded->FontSize, pos + ImVec2{ 1, 0 }, pcolor{ 0, 0, 0, 0.5f }, text.data( ), FindRenderedTextEnd( text.data( ) ) );
        draw->AddText( loaded, loaded->FontSize, pos + ImVec2{ 0, 1 }, pcolor{ 0, 0, 0, 0.5f }, text.data( ), FindRenderedTextEnd( text.data( ) ) );
    }

    draw->AddText( loaded, loaded->FontSize, pos, col, text.data( ), text_end );
}

void c_draw::textitem( int font, float fontsize, pcolor col, std::string_view text, bool shadow ) {
    this->text( font, fontsize, GetCurrentWindow( )->DC.CursorPos, col, text, 0, shadow );
    Dummy( textsize( font, fontsize, text.data( ) ) );
}

ImVec2 c_draw::textsize( int font, float fontsize, const char* text ) {
    ImFont* loaded = fonts[font].get( fontsize );
    if ( !loaded )
        loaded = ImGui::GetFont( );
    if ( !loaded )
        return ImVec2{ 0, 0 };
    return loaded->CalcTextSizeA( loaded->FontSize, FLT_MAX, -1, text, FindRenderedTextEnd( text ) );
}