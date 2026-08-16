#pragma once

#include <string>

namespace BrandPaths
{
	// Returns a writable data root ending with a backslash.
	std::string GetDataRoot();

	// HWID lock files prefer AppData so standalone retail exes work anywhere.
	std::string GetLockDataRoot();

	// Returns the directory containing the running executable.
	std::string GetExecutableDirectory();

	// Clears cached roots so a failed write can retry another location.
	void ResetCachedRoots();
}
