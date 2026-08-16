#include "PlateNative.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "../../FivemSDK/Classes.hpp"

namespace Cheat
{
	namespace PlateNative
	{
		bool SetVehiclePlate(CPed*, CVehicle*, const char*)
		{
			// Remote native invocation (CreateRemoteThread into game handlers) crashes
			// FiveM/GTA on b3258. Plate changes must go through memory writes and/or
			// the localhost bridge resource (SetVehicleNumberPlateText in client Lua).
			return false;
		}
	}
}

#else

namespace Cheat
{
	namespace PlateNative
	{
		bool SetVehiclePlate(CPed*, CVehicle*, const char*)
		{
			return false;
		}
	}
}

#endif
