#pragma once

namespace FriendsUpdate
{
	// Returns true if this process should continue. May ExitProcess after spawning a swap.
	bool TryApplyAtStartup();
}
