#pragma once

#include <string>

namespace Cheat
{
	namespace LuaExecutor
	{
		struct MachoParsedScript
		{
			std::string resource = "any";
			std::string payload;
		};

		MachoParsedScript ParseMachoScript(const std::string& code);
		std::string ResolvePayload(const std::string& code);
		bool ExecuteInClientConsole(const std::string& luaPayload);
		bool ExecuteScript(const std::string& code);
		std::string LastError();
	}
}
