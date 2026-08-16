#pragma once

#include <string>

namespace FriendsLicense
{
	bool Activate(const std::string& licenseKey, const std::string& hwidHash, std::string& outError);
}
