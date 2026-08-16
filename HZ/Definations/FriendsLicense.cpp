#include "FriendsLicense.hpp"

#if defined(FRIENDS_BUILD) && FRIENDS_BUILD

#include "FriendsCrypto.hpp"
#include "FriendsLicensePub.hpp"
#include "../Utils/BrandPaths.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace FriendsLicense
{
	namespace
	{
		std::string BindPath()
		{
			return BrandPaths::GetLockDataRoot() + "friends.bind";
		}

		bool IsRevoked(uint32_t id)
		{
			for (int i = 0; i < kFriendsRevokedCount; ++i)
			{
				if (kFriendsRevokedIds[i] == id)
					return true;
			}
			return false;
		}

		bool ReadBind(uint32_t& outId, std::string& outHwid)
		{
			std::ifstream file(BindPath(), std::ios::binary);
			if (!file)
				return false;
			std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			const size_t split = body.find('\n');
			if (split == std::string::npos)
				return false;
			outId = static_cast<uint32_t>(strtoul(body.substr(0, split).c_str(), nullptr, 10));
			outHwid = body.substr(split + 1);
			while (!outHwid.empty() && (outHwid.back() == '\r' || outHwid.back() == '\n'))
				outHwid.pop_back();
			return outId != 0 && !outHwid.empty();
		}

		bool WriteBind(uint32_t id, const std::string& hwid)
		{
			std::error_code ec;
			std::filesystem::create_directories(std::filesystem::path(BindPath()).parent_path(), ec);
			std::ofstream file(BindPath(), std::ios::binary | std::ios::trunc);
			if (!file)
				return false;
			file << id << '\n' << hwid;
			return file.good();
		}
	}

	bool Activate(const std::string& licenseKey, const std::string& hwidHash, std::string& outError)
	{
		if (kFriendsEcdsaPublicSize < 72)
		{
			outError = "HZ keygen has not been initialized on this build.";
			return false;
		}

		FriendsCrypto::Payload payload{};
		std::vector<unsigned char> sig;
		if (!FriendsCrypto::ParseKey(licenseKey, payload, sig))
		{
			outError = "Invalid key format.";
			return false;
		}

		if (!FriendsCrypto::VerifyPayload(kFriendsEcdsaPublic, kFriendsEcdsaPublicSize, payload, sig.data(), sig.size()))
		{
			outError = "Invalid or forged key.";
			return false;
		}

		if (payload.expDay != 0 && FriendsCrypto::Today() > payload.expDay)
		{
			outError = "This key has expired.";
			return false;
		}

		if (IsRevoked(payload.id))
		{
			outError = "This key has been revoked.";
			return false;
		}

		uint32_t boundId = 0;
		std::string boundHwid;
		if (ReadBind(boundId, boundHwid))
		{
			if (boundId != payload.id)
			{
				outError = "This PC is already bound to a different key.";
				return false;
			}
			if (boundHwid != hwidHash)
			{
				outError = "This key is bound to another PC.";
				return false;
			}
			return true;
		}

		if (!WriteBind(payload.id, hwidHash))
		{
			outError = "Could not save the device bind.";
			return false;
		}
		return true;
	}
}

#else

namespace FriendsLicense
{
	bool Activate(const std::string&, const std::string&, std::string& outError)
	{
		outError = "Not a friends build.";
		return false;
	}
}

#endif
