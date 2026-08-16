#pragma once

#include <string>

namespace Cheat
{
	namespace LuaProcessInject
	{
		bool IsAvailable();
		bool Execute(const std::string& luaCode, const std::string& resourceLabel);
		std::string LastError();
	}
}
