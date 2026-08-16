#include "TrinityLock.hpp"
#include "Brand.hpp"
#include "../Utils/BrandPaths.hpp"
#include "../WebControl/WebServer.hpp"

#include <Windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
#if defined(TRINITY_DEV) && TRINITY_DEV
	constexpr bool kLockEnabled = false;
	constexpr bool kEncryptLockFile = false;
#elif defined(TRINITY_HWID_LOCK) && TRINITY_HWID_LOCK
	constexpr bool kLockEnabled = true;
#if defined(TRINITY_ENCRYPT) && TRINITY_ENCRYPT
	constexpr bool kEncryptLockFile = true;
#else
	constexpr bool kEncryptLockFile = false;
#endif
#else
	constexpr bool kLockEnabled = false;
	constexpr bool kEncryptLockFile = false;
#endif

	std::string LockDirectory()
	{
		return BrandPaths::GetLockDataRoot() + "locks\\";
	}

	std::string LockFilePath()
	{
#if defined(TRINITY_BUILD_ID)
		char name[32]{};
		sprintf_s(name, "build_%02d.tlock", TRINITY_BUILD_ID);
		return LockDirectory() + name;
#else
		return LockDirectory() + "build_dev.tlock";
#endif
	}

	std::vector<unsigned char> DeriveKey()
	{
#if defined(TRINITY_BUILD_ID)
		const unsigned seed = 0x5472696Eu ^ (TRINITY_BUILD_ID * 0x9E3779B9u);
#else
		const unsigned seed = 0x5472696Eu;
#endif
		std::vector<unsigned char> key(32);
		for (size_t i = 0; i < key.size(); ++i)
			key[i] = static_cast<unsigned char>((seed >> ((i % 4) * 8)) ^ (0xA5u + i * 17u));
		return key;
	}

	std::string XorTransform(const std::string& input)
	{
		if (!kEncryptLockFile || input.empty())
			return input;

		const auto key = DeriveKey();
		std::string out = input;
		for (size_t i = 0; i < out.size(); ++i)
			out[i] = static_cast<char>(static_cast<unsigned char>(out[i]) ^ key[i % key.size()]);
		return out;
	}

	bool WriteAllText(const std::string& path, const std::string& text)
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
			return false;
		const std::string payload = XorTransform(text);
		file.write(payload.data(), static_cast<std::streamsize>(payload.size()));
		return file.good();
	}

	bool ReadAllText(const std::string& path, std::string& out)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return false;
		std::ostringstream ss;
		ss << file.rdbuf();
		out = XorTransform(ss.str());
		return true;
	}

	std::string FingerprintHwid(const std::string& hwid)
	{
		BCRYPT_ALG_HANDLE alg = nullptr;
		BCRYPT_HASH_HANDLE hash = nullptr;
		DWORD hashLen = 0, cb = 0;
		std::vector<unsigned char> buffer;
		std::vector<unsigned char> digest;

		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
			return hwid;

		if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cb, 0) != 0)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return hwid;
		}

		digest.resize(hashLen);
		if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return hwid;
		}

#if defined(TRINITY_BUILD_ID)
		const std::string material = hwid + "|trinity|" + std::to_string(TRINITY_BUILD_ID);
#else
		const std::string material = hwid + "|trinity|dev";
#endif

		if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(material.data())), static_cast<ULONG>(material.size()), 0) != 0 ||
			BCryptFinishHash(hash, digest.data(), hashLen, 0) != 0)
		{
			BCryptDestroyHash(hash);
			BCryptCloseAlgorithmProvider(alg, 0);
			return hwid;
		}

		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(alg, 0);

		static const char hex[] = "0123456789abcdef";
		std::string out;
		out.reserve(digest.size() * 2);
		for (unsigned char b : digest)
		{
			out.push_back(hex[b >> 4]);
			out.push_back(hex[b & 0xF]);
		}
		return out;
	}
}

namespace TrinityLock
{
	std::string GetBuildLabel()
	{
#if defined(HZ_DEV) && HZ_DEV
		return "DEV";
#elif defined(TRINITY_DEV) && TRINITY_DEV
		return "DEV";
#elif defined(FRIENDS_BUILD) && FRIENDS_BUILD
		return "RT";
#elif defined(TRINITY_BUILD_ID)
		char label[16]{};
		sprintf_s(label, "%02d", TRINITY_BUILD_ID);
		return label;
#else
		return "00";
#endif
	}

	bool VerifyOrBind(std::string& outError)
	{
		if (!kLockEnabled)
			return true;

		const std::string hwid = WebControl::GenerateHWID();
		if (hwid.empty())
		{
			outError = "Trinity could not read this machine identity.";
			return false;
		}

		const std::string fingerprint = FingerprintHwid(hwid);

		for (int attempt = 0; attempt < 2; ++attempt)
		{
			std::error_code ec;
			const std::string lockDir = LockDirectory();
			std::filesystem::create_directories(lockDir, ec);

			const std::string path = LockFilePath();

			if (!std::filesystem::exists(path))
			{
				if (WriteAllText(path, fingerprint))
					return true;

				if (attempt == 0)
				{
					BrandPaths::ResetCachedRoots();
					continue;
				}

				outError = "Trinity could not create the HWID lock file.\nPath: " + path +
					"\n\nRun Trinity-XX.exe from a normal user folder (not Program Files), "
					"or check that %LOCALAPPDATA%\\Trinity\\Data is writable.";
				return false;
			}

			std::string stored;
			if (!ReadAllText(path, stored))
			{
				outError = "Trinity could not read the HWID lock file.\nPath: " + path;
				return false;
			}

			if (stored != fingerprint)
			{
				outError = "This Trinity build is locked to another PC.\nBuild #" + GetBuildLabel() +
					" cannot be shared.\n\nIf you copied a Data\\locks folder from someone else, delete:\n" + path;
				return false;
			}

			return true;
		}

		outError = "Trinity could not bind this build to your PC.";
		return false;
	}
}
