#pragma once

#include <string>

namespace Cheat
{
	namespace PlateBridge
	{
		void Start();
		void Stop();
		void QueuePlate(const std::string& plate);
		bool IsRunning();
	}
}
