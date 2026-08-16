#include "DiscordAvatar.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../FrameWork/Dependencies/stb_image.h"

#include <sstream>

namespace FrameWork
{
	std::vector<unsigned char> DiscordAvatar::DownloadFromURL(const std::wstring& url)
	{
		std::vector<unsigned char> result;

		// Parse URL
		URL_COMPONENTS urlComp;
		ZeroMemory(&urlComp, sizeof(urlComp));
		urlComp.dwStructSize = sizeof(urlComp);
		
		wchar_t hostName[256];
		wchar_t urlPath[1024];
		urlComp.lpszHostName = hostName;
		urlComp.dwHostNameLength = sizeof(hostName) / sizeof(wchar_t);
		urlComp.lpszUrlPath = urlPath;
		urlComp.dwUrlPathLength = sizeof(urlPath) / sizeof(wchar_t);

		if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp))
			return result;

		// Open session
		HINTERNET hSession = WinHttpOpen(L"DiscordAvatarLoader/1.0",
			WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
			WINHTTP_NO_PROXY_NAME,
			WINHTTP_NO_PROXY_BYPASS, 0);

		if (!hSession)
			return result;

		// Connect
		HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
		if (!hConnect)
		{
			WinHttpCloseHandle(hSession);
			return result;
		}

		// Open request
		DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath,
			NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

		if (!hRequest)
		{
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return result;
		}

		// Send request
		if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return result;
		}

		// Receive response
		if (!WinHttpReceiveResponse(hRequest, NULL))
		{
			WinHttpCloseHandle(hRequest);
			WinHttpCloseHandle(hConnect);
			WinHttpCloseHandle(hSession);
			return result;
		}

		// Read data
		DWORD dwSize = 0;
		DWORD dwDownloaded = 0;
		do
		{
			dwSize = 0;
			if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
				break;

			if (dwSize == 0)
				break;

			unsigned char* buffer = new unsigned char[dwSize];
			ZeroMemory(buffer, dwSize);

			if (!WinHttpReadData(hRequest, buffer, dwSize, &dwDownloaded))
			{
				delete[] buffer;
				break;
			}

			result.insert(result.end(), buffer, buffer + dwDownloaded);
			delete[] buffer;

		} while (dwSize > 0);

		// Cleanup
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		WinHttpCloseHandle(hSession);

		return result;
	}

	std::string DiscordAvatar::GetUserAvatarHash(const std::string& userID)
	{
		// Constrói URL da API
		std::wstring apiUrl = L"https://discord.com/api/v9/users/" + std::wstring(userID.begin(), userID.end());
		
		// Baixa resposta JSON
		std::vector<unsigned char> jsonData = DownloadFromURL(apiUrl);
		if (jsonData.empty())
			return "";

		// Converte para string
		std::string jsonStr(jsonData.begin(), jsonData.end());

		// Parse simples para pegar o hash do avatar
		// Procura por "avatar":"hash"
		size_t avatarPos = jsonStr.find("\"avatar\":\"");
		if (avatarPos == std::string::npos)
			return "";

		avatarPos += 11; // Pula "avatar":"
		size_t endPos = jsonStr.find("\"", avatarPos);
		if (endPos == std::string::npos)
			return "";

		return jsonStr.substr(avatarPos, endPos - avatarPos);
	}

	bool DiscordAvatar::LoadDiscordAvatar(const std::string& userID, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
	{
		// Pega hash do avatar
		std::string avatarHash = GetUserAvatarHash(userID);
		if (avatarHash.empty())
		{
			// Se falhar, usa avatar padrão
			avatarHash = "0";
		}

		// Constrói URL do CDN
		std::wstring avatarUrl;
		if (avatarHash == "0")
		{
			// Avatar padrão
			avatarUrl = L"https://cdn.discordapp.com/embed/avatars/0.png";
		}
		else
		{
			// Avatar customizado
			std::wstring userIDW(userID.begin(), userID.end());
			std::wstring hashW(avatarHash.begin(), avatarHash.end());
			avatarUrl = L"https://cdn.discordapp.com/avatars/" + userIDW + L"/" + hashW + L".png?size=128";
		}

		// Baixa imagem
		std::vector<unsigned char> imageData = DownloadFromURL(avatarUrl);
		if (imageData.empty())
			return false;

		// Carrega textura
		return LoadTextureFromMemory(imageData.data(), imageData.size(), device, out_srv, out_width, out_height);
	}

	bool DiscordAvatar::LoadTextureFromMemory(const void* data, size_t data_size, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
	{
		// Decodifica imagem com stb_image
		int image_width = 0;
		int image_height = 0;
		unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
		if (image_data == NULL)
			return false;

		// Cria textura
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = image_width;
		desc.Height = image_height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;

		ID3D11Texture2D* pTexture = NULL;
		D3D11_SUBRESOURCE_DATA subResource;
		subResource.pSysMem = image_data;
		subResource.SysMemPitch = desc.Width * 4;
		subResource.SysMemSlicePitch = 0;
		HRESULT hr = device->CreateTexture2D(&desc, &subResource, &pTexture);

		if (FAILED(hr))
		{
			stbi_image_free(image_data);
			return false;
		}

		// Cria shader resource view
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		ZeroMemory(&srvDesc, sizeof(srvDesc));
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;
		hr = device->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
		pTexture->Release();

		if (FAILED(hr))
		{
			stbi_image_free(image_data);
			return false;
		}

		*out_width = image_width;
		*out_height = image_height;
		stbi_image_free(image_data);

		return true;
	}
}
