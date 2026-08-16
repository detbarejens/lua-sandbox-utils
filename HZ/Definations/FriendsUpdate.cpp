#include "FriendsUpdate.hpp"

#if defined(FRIENDS_BUILD) && FRIENDS_BUILD

#include "FriendsCrypto.hpp"
#include "Brand.hpp"
#include "../Utils/BrandPaths.hpp"

#include <Windows.h>
#include <winhttp.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

#ifndef FRIENDS_BUILD_VERSION
#define FRIENDS_BUILD_VERSION 1
#endif

namespace FriendsUpdate
{
	namespace
	{
		std::string Trim(std::string s)
		{
			while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
				s.pop_back();
			size_t i = 0;
			while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
				++i;
			return s.substr(i);
		}

		std::string ReadFirstLine(const std::string& path)
		{
			std::ifstream file(path);
			std::string line;
			if (!file || !std::getline(file, line))
				return {};
			return Trim(line);
		}

		std::string ExtractJsonString(const std::string& json, const char* field)
		{
			const std::string needle = std::string("\"") + field + "\":\"";
			const size_t start = json.find(needle);
			if (start == std::string::npos)
				return {};
			size_t pos = start + needle.size();
			std::string out;
			while (pos < json.size())
			{
				if (json[pos] == '\\' && pos + 1 < json.size()) { out.push_back(json[pos + 1]); pos += 2; continue; }
				if (json[pos] == '"') break;
				out.push_back(json[pos++]);
			}
			return out;
		}

		int ExtractJsonInt(const std::string& json, const char* field, int fallback)
		{
			const std::string needle = std::string("\"") + field + "\":";
			const size_t start = json.find(needle);
			if (start == std::string::npos)
				return fallback;
			return atoi(json.c_str() + start + needle.size());
		}

		bool ParseHttpsUrl(const std::string& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port)
		{
			if (url.rfind("https://", 0) != 0)
				return false;
			std::string rest = url.substr(8);
			const size_t slash = rest.find('/');
			std::string hostPort = slash == std::string::npos ? rest : rest.substr(0, slash);
			path = slash == std::string::npos ? L"/" : std::wstring(rest.begin() + slash, rest.end());
			port = INTERNET_DEFAULT_HTTPS_PORT;
			const size_t colon = hostPort.find(':');
			if (colon != std::string::npos)
			{
				port = static_cast<INTERNET_PORT>(atoi(hostPort.c_str() + colon + 1));
				hostPort = hostPort.substr(0, colon);
			}
			host.assign(hostPort.begin(), hostPort.end());
			return !host.empty();
		}

		bool HttpsGet(const std::string& url, const std::string& token, std::string& body, std::string& error)
		{
			std::wstring host, path;
			INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
			if (!ParseHttpsUrl(url, host, path, port))
			{
				error = "Update URL must be https.";
				return false;
			}

			HINTERNET session = WinHttpOpen(BRAND_HTTP_UA, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!session) { error = "Network init failed."; return false; }
			WinHttpSetTimeouts(session, 8000, 8000, 15000, 30000);

			HINTERNET connect = WinHttpConnect(session, host.c_str(), port, 0);
			if (!connect)
			{
				error = "Could not reach update host.";
				WinHttpCloseHandle(session);
				return false;
			}

			HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
			if (!request)
			{
				error = "Could not create update request.";
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return false;
			}

			std::wstring headers;
			if (!token.empty())
			{
				headers = L"Authorization: Bearer ";
				headers.append(token.begin(), token.end());
				headers += L"\r\nAccept: application/octet-stream\r\n";
			}

			if (!WinHttpSendRequest(request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
				headers.empty() ? 0 : static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
				!WinHttpReceiveResponse(request, nullptr))
			{
				error = "Update download failed.";
				WinHttpCloseHandle(request);
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return false;
			}

			DWORD status = 0, statusSize = sizeof(status);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &statusSize, nullptr);
			if (status != 200)
			{
				error = "Update host returned HTTP " + std::to_string(status);
				WinHttpCloseHandle(request);
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return false;
			}

			body.clear();
			char buf[4096];
			DWORD read = 0;
			while (WinHttpReadData(request, buf, sizeof(buf), &read) && read > 0)
				body.append(buf, read);

			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
			return true;
		}

		std::string HexLower(const unsigned char* data, size_t len)
		{
			static const char* hex = "0123456789abcdef";
			std::string out;
			out.resize(len * 2);
			for (size_t i = 0; i < len; ++i)
			{
				out[i * 2] = hex[data[i] >> 4];
				out[i * 2 + 1] = hex[data[i] & 0xF];
			}
			return out;
		}

		bool Sha256File(const std::string& path, std::string& hex)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file)
				return false;
			std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			unsigned char digest[32]{};
			if (!FriendsCrypto::Sha256(bytes.data(), bytes.size(), digest))
				return false;
			hex = HexLower(digest, 32);
			return true;
		}

		void LaunchSwap(const std::string& exePath, const std::string& newPath)
		{
			const std::string batPath = exePath + ".update.bat";
			std::ofstream bat(batPath);
			if (!bat)
				return;
			bat << "@echo off\r\n"
				<< "timeout /t 2 /nobreak >nul\r\n"
				<< "del /f \"" << exePath << "\" >nul 2>&1\r\n"
				<< "move /y \"" << newPath << "\" \"" << exePath << "\" >nul\r\n"
				<< "start \"\" \"" << exePath << "\"\r\n"
				<< "del \"%~f0\"\r\n";
			bat.close();

			STARTUPINFOA si{};
			si.cb = sizeof(si);
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = SW_HIDE;
			PROCESS_INFORMATION pi{};
			std::string cmd = "cmd.exe /C \"" + batPath + "\"";
			if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
			{
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			}
		}
	}

	bool TryApplyAtStartup()
	{
		const std::string exeDir = BrandPaths::GetExecutableDirectory();
		if (exeDir.empty())
			return true;

		const std::string manifestUrl = ReadFirstLine(exeDir + "update_url.txt");
		if (manifestUrl.empty())
			return true;

		const std::string token = ReadFirstLine(exeDir + "github_token.txt");
		std::string json, error;
		if (!HttpsGet(manifestUrl, token, json, error))
			return true;

		const int remoteVersion = ExtractJsonInt(json, "version", 0);
		if (remoteVersion <= FRIENDS_BUILD_VERSION)
			return true;

		const std::string shaRaw = ExtractJsonString(json, "sha256");
		std::string sha = shaRaw;
		for (char& c : sha)
		{
			if (c >= 'A' && c <= 'F')
				c = static_cast<char>(c - 'A' + 'a');
		}
		const std::string fileUrl = ExtractJsonString(json, "url");
		if (sha.size() != 64 || fileUrl.empty())
			return true;

		std::string payload;
		if (!HttpsGet(fileUrl, token, payload, error) || payload.empty())
			return true;

		unsigned char digest[32]{};
		if (!FriendsCrypto::Sha256(payload.data(), payload.size(), digest))
			return true;
		if (HexLower(digest, 32) != sha)
			return true;

		char exePath[MAX_PATH]{};
		GetModuleFileNameA(nullptr, exePath, MAX_PATH);
		const std::string newPath = std::string(exePath) + ".new";
		{
			std::ofstream out(newPath, std::ios::binary | std::ios::trunc);
			if (!out)
				return true;
			out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
		}

		std::string fileHex;
		if (!Sha256File(newPath, fileHex) || fileHex != sha)
		{
			std::error_code ec;
			std::filesystem::remove(newPath, ec);
			return true;
		}

		LaunchSwap(exePath, newPath);
		ExitProcess(0);
		return false;
	}
}

#else

namespace FriendsUpdate
{
	bool TryApplyAtStartup()
	{
		return true;
	}
}

#endif
