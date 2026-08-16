#include "LuaExecutor.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "LuaProcessInject.hpp"
#include "LuaRuntime.hpp"
#include "../../Utils/Memory.hpp"

#include <Windows.h>

#include <cctype>
#include <regex>

namespace Cheat
{
	namespace LuaExecutor
	{
		namespace
		{
			constexpr size_t kMaxAutoExecBytes = 96 * 1024;

			void SkipWhitespace(const std::string& text, size_t& index)
			{
				while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])))
					++index;
			}

			bool ParseQuotedLuaString(const std::string& text, size_t& index, std::string& out)
			{
				SkipWhitespace(text, index);
				if (index >= text.size())
					return false;

				const char quote = text[index];
				if (quote != '\'' && quote != '"')
					return false;

				++index;
				out.clear();
				while (index < text.size())
				{
					if (text[index] == '\\' && index + 1 < text.size())
					{
						out += text[index + 1];
						index += 2;
						continue;
					}

					if (text[index] == quote)
					{
						++index;
						return true;
					}

					out += text[index++];
				}

				return false;
			}

			bool ParseLongBracketLuaString(const std::string& text, size_t& index, std::string& out)
			{
				SkipWhitespace(text, index);
				if (index >= text.size() || text[index] != '[')
					return false;

				size_t equalsCount = 0;
				size_t cursor = index + 1;
				while (cursor < text.size() && text[cursor] == '=')
				{
					++equalsCount;
					++cursor;
				}

				if (cursor >= text.size() || text[cursor] != '[')
					return false;

				++cursor;
				std::string closing = "]";
				closing.append(equalsCount, '=');
				closing += ']';

				const size_t contentStart = cursor;
				const size_t contentEnd = text.find(closing, contentStart);
				if (contentEnd == std::string::npos)
					return false;

				out = text.substr(contentStart, contentEnd - contentStart);
				index = contentEnd + closing.size();
				return true;
			}

			bool ParseLuaStringArg(const std::string& text, size_t& index, std::string& out)
			{
				SkipWhitespace(text, index);
				if (index < text.size() && text[index] == '[')
					return ParseLongBracketLuaString(text, index, out);
				return ParseQuotedLuaString(text, index, out);
			}

			MachoParsedScript ParseMachoScriptInternal(const std::string& code)
			{
				MachoParsedScript parsed;
				static const std::regex machoHead(
					R"(^\s*MachoInjectResource(?:Raw|2)?\s*\()",
					std::regex::icase);
				if (!std::regex_search(code, machoHead))
				{
					parsed.payload = code;
					return parsed;
				}

				const size_t openParen = code.find('(');
				if (openParen == std::string::npos)
				{
					parsed.payload = code;
					return parsed;
				}

				const bool isMacho2 = code.find("MachoInjectResource2") != std::string::npos;

				size_t index = openParen + 1;
				std::string firstArg;
				if (!ParseLuaStringArg(code, index, firstArg))
				{
					parsed.payload = code;
					return parsed;
				}

				SkipWhitespace(code, index);
				if (index < code.size() && code[index] == ',')
					++index;

				if (isMacho2)
				{
					std::string typeArg;
					if (!ParseLuaStringArg(code, index, typeArg))
					{
						parsed.payload = code;
						return parsed;
					}

					SkipWhitespace(code, index);
					if (index < code.size() && code[index] == ',')
						++index;

					if (!ParseLuaStringArg(code, index, parsed.resource))
					{
						parsed.payload = code;
						return parsed;
					}

					SkipWhitespace(code, index);
					if (index < code.size() && code[index] == ',')
						++index;

					if (!ParseLuaStringArg(code, index, parsed.payload))
						parsed.payload = code;
				}
				else
				{
					parsed.resource = firstArg;
					if (!ParseLuaStringArg(code, index, parsed.payload))
						parsed.payload = code;
				}

				return parsed;
			}

			bool CopyTextToClipboard(const std::string& text)
			{
				if (text.empty() || !OpenClipboard(nullptr))
					return false;

				EmptyClipboard();
				const size_t size = text.size() + 1;
				HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
				if (!memory)
				{
					CloseClipboard();
					return false;
				}

				void* locked = GlobalLock(memory);
				if (!locked)
				{
					GlobalFree(memory);
					CloseClipboard();
					return false;
				}

				memcpy(locked, text.c_str(), size);
				GlobalUnlock(memory);
				SetClipboardData(CF_TEXT, memory);
				CloseClipboard();
				return true;
			}

			void TapKey(WORD virtualKey)
			{
				INPUT down{};
				down.type = INPUT_KEYBOARD;
				down.ki.wVk = virtualKey;
				SendInput(1, &down, sizeof(INPUT));

				INPUT up = down;
				up.ki.dwFlags = KEYEVENTF_KEYUP;
				SendInput(1, &up, sizeof(INPUT));
			}

			void HoldChordPaste()
			{
				INPUT inputs[4]{};
				inputs[0].type = INPUT_KEYBOARD;
				inputs[0].ki.wVk = VK_CONTROL;
				inputs[1].type = INPUT_KEYBOARD;
				inputs[1].ki.wVk = 'V';
				inputs[2].type = INPUT_KEYBOARD;
				inputs[2].ki.wVk = 'V';
				inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
				inputs[3].type = INPUT_KEYBOARD;
				inputs[3].ki.wVk = VK_CONTROL;
				inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
				SendInput(4, inputs, sizeof(INPUT));
			}

			bool FocusWindow(HWND window)
			{
				if (!window)
					return false;

				HWND foreground = GetForegroundWindow();
				DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
				DWORD targetThread = GetWindowThreadProcessId(window, nullptr);
				const DWORD currentThread = GetCurrentThreadId();

				if (foregroundThread && foregroundThread != currentThread)
					AttachThreadInput(currentThread, foregroundThread, TRUE);
				if (targetThread && targetThread != currentThread)
					AttachThreadInput(currentThread, targetThread, TRUE);

				ShowWindow(window, SW_RESTORE);
				BringWindowToTop(window);
				SetForegroundWindow(window);

				if (targetThread && targetThread != currentThread)
					AttachThreadInput(currentThread, targetThread, FALSE);
				if (foregroundThread && foregroundThread != currentThread)
					AttachThreadInput(currentThread, foregroundThread, FALSE);

				return GetForegroundWindow() == window;
			}
		}

		MachoParsedScript ParseMachoScript(const std::string& code)
		{
			return ParseMachoScriptInternal(code);
		}

		std::string ResolvePayload(const std::string& code)
		{
			return ParseMachoScriptInternal(code).payload;
		}

		bool ExecuteInClientConsole(const std::string& luaPayload)
		{
			if (luaPayload.empty() || luaPayload.size() > kMaxAutoExecBytes)
				return false;

			if (!FrameWork::Memory::AttachedProcessHandle)
				return false;

			const HWND gameWindow = FrameWork::Memory::FindGameWindow(FrameWork::Memory::AttachedProcessPid);
			if (!gameWindow)
				return false;

			if (!CopyTextToClipboard(luaPayload))
				return false;

			if (!FocusWindow(gameWindow))
				return false;

			Sleep(120);
			TapKey(VK_F8);
			Sleep(220);
			HoldChordPaste();
			Sleep(120);
			TapKey(VK_RETURN);
			Sleep(120);
			TapKey(VK_F8);

			return true;
		}

		bool ExecuteScript(const std::string& code)
		{
			const MachoParsedScript parsed = ParseMachoScriptInternal(code);
			if (parsed.payload.empty())
				return false;

			const std::string resource = LuaRuntime::ResolveInjectResource(parsed.resource, parsed.payload);
			return FrameWork::Memory::AttachedProcessHandle && LuaProcessInject::Execute(parsed.payload, resource);
		}

		std::string LastError()
		{
			return LuaProcessInject::LastError();
		}
	}
}

#else

namespace Cheat
{
	namespace LuaExecutor
	{
		MachoParsedScript ParseMachoScript(const std::string& code)
		{
			return { "any", code };
		}

		std::string ResolvePayload(const std::string& code) { return code; }
		bool ExecuteInClientConsole(const std::string&) { return false; }
		bool ExecuteScript(const std::string&) { return false; }
		std::string LastError() { return "Dev executor is not in this build."; }
	}
}

#endif
