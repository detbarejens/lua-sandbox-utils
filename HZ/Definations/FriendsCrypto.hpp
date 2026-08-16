#pragma once

#include <Windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "bcrypt.lib")

namespace FriendsCrypto
{
	constexpr uint8_t kPayloadVersion = 1;
	constexpr size_t kPayloadSize = 9;
	constexpr size_t kSignatureSize = 64;
	constexpr ULONG kEcdsaPubMagic = 0x31534345; // BCRYPT_ECDSA_PUBLIC_P256_MAGIC
	constexpr ULONG kEcdsaPrivMagic = 0x32534345; // BCRYPT_ECDSA_PRIVATE_P256_MAGIC

#pragma pack(push, 1)
	struct Payload
	{
		uint8_t version;
		uint32_t id;
		uint32_t expDay; // unix seconds / 86400, 0 = never
	};
#pragma pack(pop)

	static_assert(sizeof(Payload) == kPayloadSize, "payload size");

	inline std::string Base64UrlEncode(const unsigned char* data, size_t len)
	{
		static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
		std::string out;
		out.reserve(((len + 2) / 3) * 4);
		for (size_t i = 0; i < len; i += 3)
		{
			unsigned n = data[i] << 16;
			if (i + 1 < len) n |= data[i + 1] << 8;
			if (i + 2 < len) n |= data[i + 2];
			out.push_back(table[(n >> 18) & 63]);
			out.push_back(table[(n >> 12) & 63]);
			if (i + 1 < len) out.push_back(table[(n >> 6) & 63]);
			if (i + 2 < len) out.push_back(table[n & 63]);
		}
		return out;
	}

	inline bool Base64UrlDecode(const std::string& in, std::vector<unsigned char>& out)
	{
		auto val = [](char c) -> int {
			if (c >= 'A' && c <= 'Z') return c - 'A';
			if (c >= 'a' && c <= 'z') return c - 'a' + 26;
			if (c >= '0' && c <= '9') return c - '0' + 52;
			if (c == '-') return 62;
			if (c == '_') return 63;
			return -1;
		};

		out.clear();
		int acc = 0;
		int bits = 0;
		for (char c : in)
		{
			if (c == '=' || c == '\r' || c == '\n' || c == ' ')
				continue;
			const int v = val(c);
			if (v < 0)
				return false;
			acc = (acc << 6) | v;
			bits += 6;
			if (bits >= 8)
			{
				bits -= 8;
				out.push_back(static_cast<unsigned char>((acc >> bits) & 0xFF));
			}
		}
		return true;
	}

	inline bool Sha256(const void* data, size_t len, unsigned char out[32])
	{
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
			return false;

		DWORD hashLen = 0, cb = 0;
		if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cb, 0) != 0 || hashLen != 32)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return false;
		}

		BCRYPT_HASH_HANDLE hash = nullptr;
		if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0)
		{
			BCryptCloseAlgorithmProvider(alg, 0);
			return false;
		}

		const bool ok =
			BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void*>(data)), static_cast<ULONG>(len), 0) == 0 &&
			BCryptFinishHash(hash, out, 32, 0) == 0;

		BCryptDestroyHash(hash);
		BCryptCloseAlgorithmProvider(alg, 0);
		return ok;
	}

	inline bool WriteFileBytes(const std::string& path, const std::vector<unsigned char>& bytes)
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		if (!file)
			return false;
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		return file.good();
	}

	inline bool ReadFileBytes(const std::string& path, std::vector<unsigned char>& bytes)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file)
			return false;
		bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		return !bytes.empty();
	}

	inline bool ExportKeyBlob(BCRYPT_KEY_HANDLE key, LPCWSTR blobType, std::vector<unsigned char>& out)
	{
		DWORD cb = 0;
		if (BCryptExportKey(key, nullptr, blobType, nullptr, 0, &cb, 0) != 0)
			return false;
		out.resize(cb);
		return BCryptExportKey(key, nullptr, blobType, out.data(), cb, &cb, 0) == 0;
	}

	inline bool GenerateKeyPair(std::vector<unsigned char>& privBlob, std::vector<unsigned char>& pubBlob)
	{
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
			return false;

		BCRYPT_KEY_HANDLE key = nullptr;
		bool ok = BCryptGenerateKeyPair(alg, &key, 256, 0) == 0 && BCryptFinalizeKeyPair(key, 0) == 0;
		if (ok)
			ok = ExportKeyBlob(key, BCRYPT_ECCPRIVATE_BLOB, privBlob) && ExportKeyBlob(key, BCRYPT_ECCPUBLIC_BLOB, pubBlob);

		if (key) BCryptDestroyKey(key);
		BCryptCloseAlgorithmProvider(alg, 0);
		return ok;
	}

	inline bool ImportKey(LPCWSTR blobType, const unsigned char* blob, size_t blobLen, BCRYPT_KEY_HANDLE& outKey)
	{
		BCRYPT_ALG_HANDLE alg = nullptr;
		if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0)
			return false;

		const NTSTATUS st = BCryptImportKeyPair(
			alg, nullptr, blobType, &outKey,
			const_cast<PUCHAR>(blob), static_cast<ULONG>(blobLen), 0);
		BCryptCloseAlgorithmProvider(alg, 0);
		return st == 0;
	}

	inline bool SignPayload(const unsigned char* privBlob, size_t privLen, const Payload& payload, unsigned char sig[kSignatureSize])
	{
		BCRYPT_KEY_HANDLE key = nullptr;
		if (!ImportKey(BCRYPT_ECCPRIVATE_BLOB, privBlob, privLen, key))
			return false;

		unsigned char hash[32]{};
		const bool hashed = Sha256(&payload, sizeof(payload), hash);
		DWORD cb = kSignatureSize;
		const bool ok = hashed && BCryptSignHash(key, nullptr, hash, 32, sig, kSignatureSize, &cb, 0) == 0 && cb == kSignatureSize;
		BCryptDestroyKey(key);
		return ok;
	}

	inline bool VerifyPayload(const unsigned char* pubBlob, size_t pubLen, const Payload& payload, const unsigned char* sig, size_t sigLen)
	{
		if (sigLen != kSignatureSize)
			return false;

		BCRYPT_KEY_HANDLE key = nullptr;
		if (!ImportKey(BCRYPT_ECCPUBLIC_BLOB, pubBlob, pubLen, key))
			return false;

		unsigned char hash[32]{};
		const bool hashed = Sha256(&payload, sizeof(payload), hash);
		const bool ok = hashed && BCryptVerifySignature(key, nullptr, hash, 32, const_cast<PUCHAR>(sig), static_cast<ULONG>(sigLen), 0) == 0;
		BCryptDestroyKey(key);
		return ok;
	}

	inline std::string FormatKey(const Payload& payload, const unsigned char sig[kSignatureSize])
	{
		return "HZ1." + Base64UrlEncode(reinterpret_cast<const unsigned char*>(&payload), sizeof(payload)) + "." +
			Base64UrlEncode(sig, kSignatureSize);
	}

	inline std::string StripKey(const std::string& raw)
	{
		std::string out;
		out.reserve(raw.size());
		for (char c : raw)
		{
			if (c != ' ' && c != '\r' && c != '\n' && c != '\t')
				out.push_back(c);
		}
		return out;
	}

	inline bool ParseKey(const std::string& raw, Payload& payload, std::vector<unsigned char>& sig)
	{
		const std::string key = StripKey(raw);
		if (key.rfind("HZ1.", 0) != 0)
			return false;

		const size_t dot = key.find('.', 4);
		if (dot == std::string::npos)
			return false;

		std::vector<unsigned char> payloadBytes;
		if (!Base64UrlDecode(key.substr(5, dot - 5), payloadBytes) || payloadBytes.size() != sizeof(Payload))
			return false;
		if (!Base64UrlDecode(key.substr(dot + 1), sig) || sig.size() != kSignatureSize)
			return false;

		memcpy(&payload, payloadBytes.data(), sizeof(Payload));
		return payload.version == kPayloadVersion;
	}

	inline uint32_t Today()
	{
		return static_cast<uint32_t>(time(nullptr) / 86400);
	}
}
