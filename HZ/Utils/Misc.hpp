#pragma once

#include "../FiveM-External.hpp"
#include <Windows.h>
#include <string>
#include "../ImGui/imgui.h"

namespace FrameWork
{
	namespace Misc
	{
		int RandomInt(int Lower = -100, int Max = 100);
		int GenRandomInt(int min, int max);
		std::wstring RandomString(size_t Length = 12);
		std::pair<ImVec2, ImVec2> GetWindowPosAndSize(HWND window);
		std::string Wstring2String(std::wstring Input);
		std::wstring String2WString(std::string Input);
		void InitializeConsole();
		void ShutDownConsole();
		ImColor Float4ToImColor(float* Input);
		ImVec2 CalcTextSize(ImFont* Font, int Size, const char* Label);
	}
}