#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <string>
#include <vector>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")

namespace WebControl
{
	struct Vec2 { float x, y; };
	struct Vec4 { float r, g, b, a; };

	class OverlayRenderer
	{
	public:
		OverlayRenderer();
		~OverlayRenderer();

		bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
		void BeginFrame();
		void EndFrame();
		void Shutdown();

		// Drawing primitives (replaces ImGui draw list)
		void DrawLine(Vec2 from, Vec2 to, Vec4 color, float thickness = 1.f);
		void DrawRect(Vec2 pos, Vec2 size, Vec4 color, float thickness = 1.f);
		void DrawRectFilled(Vec2 pos, Vec2 size, Vec4 color);
		void DrawCircle(Vec2 center, float radius, Vec4 color, int segments = 32, float thickness = 1.f);
		void DrawCircleFilled(Vec2 center, float radius, Vec4 color, int segments = 32);
		void DrawText(const std::string& text, Vec2 pos, Vec4 color, float size = 16.f);

		// Health bar
		void DrawHealthBar(Vec2 pos, Vec2 size, float health, bool vertical = true);

	private:
		ID3D11Device* device;
		ID3D11DeviceContext* context;

		// Vertex buffer for line rendering
		struct Vertex
		{
			float x, y, z;
			float r, g, b, a;
		};

		ID3D11Buffer* vertexBuffer;
		ID3D11VertexShader* vertexShader;
		ID3D11PixelShader* pixelShader;
		ID3D11InputLayout* inputLayout;
		ID3D11BlendState* blendState;

		bool CreateShaders();
		bool CreateBuffers();
		void RenderVertices(const std::vector<Vertex>& vertices, D3D11_PRIMITIVE_TOPOLOGY topology);
	};
}
