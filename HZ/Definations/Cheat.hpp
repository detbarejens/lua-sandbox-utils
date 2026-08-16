#pragma once

// Math and Memory must come before FivemSDK classes
#include "../Math/Vectors/Vector2D.hpp"
#include "../Math/Vectors/Vector3D.hpp"
#include "../Math/Vectors/Vector4D.hpp"
#include "../Utils/Memory.hpp"

#include "../FivemSDK/Fivem.hpp"
#include "../FivemSDK/Offsets.hpp"

#include "../Features/Visuals/Player/PlayerESP.hpp"
#include "../Features/Visuals/Player/ObserverAlert.hpp"
#include "../Features/Visuals/Vehicle/VehicleESP.hpp"

#include "../Features/Misc/Exploits.hpp"
#include "../Features/LegitBot/AimBot.hpp"
#include "../Features/LegitBot/SilentAim.hpp"
#include "../Features/LegitBot/TriggerBot.hpp"

#include "Variables.hpp"

namespace Cheat
{
	void InstallExceptionHandlers();
	void Initialize();
	void ShutDown();
	void StartFeatureThreads();
}

#if defined(LICENSE_AUTH) && LICENSE_AUTH
#include "Auth.hpp"
#endif