#pragma once

#include <Windows.h>
#include <winhttp.h>
#include <d3d11.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace FrameWork
{
	class DiscordAvatar
	{
	public:
		// Baixa dados de uma URL usando WinHTTP
		static std::vector<unsigned char> DownloadFromURL(const std::wstring& url);
		
		// Pega informações do usuário Discord via API
		static std::string GetUserAvatarHash(const std::string& userID);
		
		// Carrega avatar do Discord direto na memória
		static bool LoadDiscordAvatar(const std::string& userID, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
		
		// Carrega textura DX11 de dados em memória
		static bool LoadTextureFromMemory(const void* data, size_t data_size, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
	};
}
