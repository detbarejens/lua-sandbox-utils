#pragma once

#include "../FivemSDK/Fivem.hpp"

namespace FrameWork
{
	namespace Visibility
	{
		int checkpixels(Cheat::Entity& Current);
		bool IsPlayerVisible(Cheat::Entity& Current, int threshold = 150);
		bool IsVehicleVisible(Cheat::CVehicle* Vehicle, int threshold = 10);
	}
}
