#pragma once

#include <string>
#include <atomic>
#include <mutex>

namespace Cheat
{
	struct AuthState
	{
		std::atomic<bool> Authenticated{ false };
		std::atomic<bool> Authenticating{ false };
		std::atomic<bool> AuthUiSuccess{ false };
		std::mutex ErrorMutex;
		std::string PendingError;
		std::string LastError;
		std::string LicenseKey;
		std::string HwidHash;
	};

	extern AuthState g_AuthState;

	std::string GetHwidHash();
	bool TryAuthenticateWithKey(const std::string& licenseKey, std::string& outError);
	void BeginAuthenticateWithKeyAsync(const std::string& licenseKey);
	void PollAuthUiState(bool& outSuccess, std::string& outError, bool& outBusy);
	void SaveLicenseKey(const std::string& licenseKey);
	bool LoadSavedLicenseKey(std::string& outKey);
	void StartWebControlAfterAuth();
}
