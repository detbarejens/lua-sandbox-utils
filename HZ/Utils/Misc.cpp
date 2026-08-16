#include "Misc.hpp"

#include <algorithm>
#include <sstream>
#include <tchar.h>
#include <random>
#include <regex>
#include <psapi.h>

namespace FrameWork
{
	int Misc::RandomInt(int Lower, int Max)
	{
		return (rand() % (Max - Lower + 1)) + Lower;
	}

	int Misc::GenRandomInt(int min, int max) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> distrib(min, max);
		return distrib(gen);
	}

	std::wstring Misc::RandomString(size_t Length)
	{
		auto Randchar = []() -> char
			{
				const char* Charset = XorStr("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
				const size_t MaxIndex = (sizeof(Charset) - 1);
				return Charset[rand() % MaxIndex];
			};

		std::wstring Str(Length, 0);
		std::generate_n(Str.begin(), Length, Randchar);
		return Str;
	}

	std::string Misc::Wstring2String(std::wstring Input)
	{
		return std::string(Input.begin(), Input.end());
	}

	std::wstring Misc::String2WString(std::string Input)
	{
		return std::wstring(Input.begin(), Input.end());
	}

	std::pair<ImVec2, ImVec2> Misc::GetWindowPosAndSize(HWND window) {
		RECT rect;
		if (GetWindowRect(window, &rect)) {
			int x = rect.left;
			int y = rect.top;

			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;

			DWORD dwStyle = GetWindowLongPtr(window, GWL_STYLE);
			if (dwStyle & WS_BORDER) {
				x += 8;
				y += 32;
				width -= 16;
				height -= 39;
			}

			ImVec2 position{ (float)x, (float)y };
			ImVec2 size{ (float)width, (float)height };
			return std::make_pair(position, size);
		}
		else {
			return std::make_pair(ImVec2{ 0, 0 }, ImVec2{ 0, 0 });
		}
	}

	void Misc::InitializeConsole()
	{
		SafeCall(AllocConsole)();
		freopen(XorStr("CONIN$"), XorStr("r"), stdin);
		freopen(XorStr("CONOUT$"), XorStr("w"), stdout);
		SafeCall(SetConsoleTitleA)(XorStr("Debug Console"));
	}

	ImVec2 Misc::CalcTextSize(ImFont* Font, int Size, const char* Label)
	{
		return Font->CalcTextSizeA(Size, FLT_MAX, 0, Label);
	}

	void Misc::ShutDownConsole()
	{
		SafeCall(FreeConsole)();
	}

	ImColor Misc::Float4ToImColor(float* Input)
	{
		return ImColor(Input[0], Input[1], Input[2], Input[3]);
	}
}