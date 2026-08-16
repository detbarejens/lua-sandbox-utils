#pragma once

#include <string>

namespace TrinityLock
{
	// Returns false and sets outError when this build is locked to another machine.
	bool VerifyOrBind(std::string& outError);

	std::string GetBuildLabel();
}
