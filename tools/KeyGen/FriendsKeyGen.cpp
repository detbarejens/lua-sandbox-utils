#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../HZ/Definations/FriendsCrypto.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{
	struct KeyRecord
	{
		uint32_t id = 0;
		uint32_t expDay = 0;
		bool revoked = false;
		std::string note;
		std::string key;
	};

	struct Store
	{
		uint32_t nextId = 1;
		std::vector<KeyRecord> keys;
	};

	std::string ToolDir()
	{
		char path[MAX_PATH]{};
		GetModuleFileNameA(nullptr, path, MAX_PATH);
		return fs::path(path).parent_path().string();
	}

	std::string Join(const std::string& dir, const char* name)
	{
		return (fs::path(dir) / name).string();
	}

	std::string HeaderPath()
	{
		fs::path p = fs::path(ToolDir()) / ".." / ".." / "HZ" / "Definations" / "FriendsLicensePub.hpp";
		std::error_code ec;
		const auto canonical = fs::weakly_canonical(p, ec);
		return ec ? p.string() : canonical.string();
	}

	std::string JsonEscape(const std::string& in)
	{
		std::string out;
		out.reserve(in.size() + 8);
		for (char c : in)
		{
			if (c == '\\' || c == '"') out.push_back('\\');
			out.push_back(c);
		}
		return out;
	}

	std::string JsonUnescape(const std::string& in)
	{
		std::string out;
		out.reserve(in.size());
		for (size_t i = 0; i < in.size(); ++i)
		{
			if (in[i] == '\\' && i + 1 < in.size())
			{
				out.push_back(in[i + 1]);
				++i;
			}
			else
				out.push_back(in[i]);
		}
		return out;
	}

	std::string ExtractObjectString(const std::string& obj, const char* field)
	{
		const std::string needle = std::string("\"") + field + "\":";
		const size_t start = obj.find(needle);
		if (start == std::string::npos)
			return {};
		size_t pos = start + needle.size();
		while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t'))
			++pos;
		if (pos >= obj.size() || obj[pos] != '"')
			return {};
		++pos;
		std::string raw;
		while (pos < obj.size())
		{
			if (obj[pos] == '\\' && pos + 1 < obj.size()) { raw.push_back('\\'); raw.push_back(obj[pos + 1]); pos += 2; continue; }
			if (obj[pos] == '"') break;
			raw.push_back(obj[pos++]);
		}
		return JsonUnescape(raw);
	}

	uint32_t ExtractObjectUInt(const std::string& obj, const char* field, uint32_t fallback = 0)
	{
		const std::string needle = std::string("\"") + field + "\":";
		const size_t start = obj.find(needle);
		if (start == std::string::npos)
			return fallback;
		return static_cast<uint32_t>(strtoul(obj.c_str() + start + needle.size(), nullptr, 10));
	}

	bool ExtractObjectBool(const std::string& obj, const char* field)
	{
		const std::string needle = std::string("\"") + field + "\":";
		const size_t start = obj.find(needle);
		if (start == std::string::npos)
			return false;
		size_t pos = start + needle.size();
		while (pos < obj.size() && (obj[pos] == ' ' || obj[pos] == '\t'))
			++pos;
		return obj.compare(pos, 4, "true") == 0;
	}

	bool LoadStore(Store& store)
	{
		store = {};
		store.nextId = 1;
		std::ifstream file(Join(ToolDir(), "friends-keys.json"), std::ios::binary);
		if (!file)
			return true;

		const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		store.nextId = ExtractObjectUInt(json, "nextId", 1);
		if (store.nextId == 0)
			store.nextId = 1;

		const size_t keysPos = json.find("\"keys\"");
		if (keysPos == std::string::npos)
			return true;

		size_t pos = json.find('[', keysPos);
		if (pos == std::string::npos)
			return true;

		while (pos < json.size())
		{
			const size_t objStart = json.find('{', pos);
			if (objStart == std::string::npos)
				break;
			const size_t objEnd = json.find('}', objStart);
			if (objEnd == std::string::npos)
				break;
			const std::string obj = json.substr(objStart, objEnd - objStart + 1);
			KeyRecord rec;
			rec.id = ExtractObjectUInt(obj, "id");
			rec.expDay = ExtractObjectUInt(obj, "expDay");
			rec.revoked = ExtractObjectBool(obj, "revoked");
			rec.note = ExtractObjectString(obj, "note");
			rec.key = ExtractObjectString(obj, "key");
			if (rec.id != 0)
				store.keys.push_back(rec);
			pos = objEnd + 1;
			const size_t bracket = json.find(']', objEnd);
			const size_t nextObj = json.find('{', objEnd);
			if (bracket != std::string::npos && (nextObj == std::string::npos || bracket < nextObj))
				break;
		}
		return true;
	}

	bool SaveStore(const Store& store)
	{
		std::ofstream file(Join(ToolDir(), "friends-keys.json"), std::ios::binary | std::ios::trunc);
		if (!file)
			return false;
		file << "{\n  \"nextId\": " << store.nextId << ",\n  \"keys\": [\n";
		for (size_t i = 0; i < store.keys.size(); ++i)
		{
			const auto& k = store.keys[i];
			file << "    {\"id\": " << k.id
				<< ", \"expDay\": " << k.expDay
				<< ", \"revoked\": " << (k.revoked ? "true" : "false")
				<< ", \"note\": \"" << JsonEscape(k.note)
				<< "\", \"key\": \"" << JsonEscape(k.key) << "\"}";
			if (i + 1 < store.keys.size())
				file << ",";
			file << "\n";
		}
		file << "  ]\n}\n";
		return file.good();
	}

	bool WritePublicHeader(const std::vector<unsigned char>& pub, const Store& store)
	{
		std::ofstream out(HeaderPath(), std::ios::trunc);
		if (!out)
		{
			std::cerr << "Could not write " << HeaderPath() << "\n";
			return false;
		}

		out << "#pragma once\n\n";
		out << "// Generated by tools/FriendsKeyGen. Public key is safe to commit; the private key is not.\n";
		out << "// Run: tools\\FriendsKeyGen\\FriendsKeyGen.bat init\n\n";
		out << "static const unsigned char kFriendsEcdsaPublic[] = {\n\t";
		for (size_t i = 0; i < pub.size(); ++i)
		{
			char buf[8]{};
			sprintf_s(buf, "0x%02X", pub[i]);
			out << buf;
			if (i + 1 < pub.size())
				out << ", ";
			if ((i + 1) % 12 == 0 && i + 1 < pub.size())
				out << "\n\t";
		}
		out << "\n};\n";
		out << "static const unsigned int kFriendsEcdsaPublicSize = " << static_cast<unsigned>(pub.size()) << ";\n\n";

		std::vector<uint32_t> revoked;
		for (const auto& k : store.keys)
		{
			if (k.revoked)
				revoked.push_back(k.id);
		}

		out << "static const unsigned int kFriendsRevokedIds[] = { ";
		if (revoked.empty())
			out << "0";
		else
		{
			for (size_t i = 0; i < revoked.size(); ++i)
			{
				if (i)
					out << ", ";
				out << revoked[i];
			}
		}
		out << " };\n";
		out << "static const int kFriendsRevokedCount = " << static_cast<int>(revoked.size()) << ";\n";
		return out.good();
	}

	bool LoadPrivate(std::vector<unsigned char>& priv)
	{
		return FriendsCrypto::ReadFileBytes(Join(ToolDir(), "friends-private.bin"), priv);
	}

	bool LoadPublic(std::vector<unsigned char>& pub)
	{
		return FriendsCrypto::ReadFileBytes(Join(ToolDir(), "friends-public.bin"), pub);
	}

	std::string FormatDay(uint32_t expDay)
	{
		if (expDay == 0)
			return "never";
		const time_t t = static_cast<time_t>(expDay) * 86400;
		struct tm tm{};
		if (gmtime_s(&tm, &t) != 0)
			return std::to_string(expDay);
		char buf[32]{};
		strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
		return buf;
	}

	void PrintUsage()
	{
		std::cout
			<< "HZ keygen (offline ECDSA, no server)\n\n"
			<< "  FriendsKeyGen init [--force]\n"
			<< "  FriendsKeyGen key [--days N] [--note TEXT] [--count N]\n"
			<< "  FriendsKeyGen list\n"
			<< "  FriendsKeyGen revoke <id>\n"
			<< "  FriendsKeyGen verify <key>\n\n"
			<< "Private key: friends-private.bin (never share / never commit)\n"
			<< "Public header is rewritten on init and revoke. Rebuild Trinity-Friends after those.\n";
	}

	int CmdInit(bool force)
	{
		const std::string privPath = Join(ToolDir(), "friends-private.bin");
		if (fs::exists(privPath) && !force)
		{
			std::cerr << "friends-private.bin already exists. Pass --force to rotate (invalidates every issued key).\n";
			return 1;
		}

		std::vector<unsigned char> priv, pub;
		if (!FriendsCrypto::GenerateKeyPair(priv, pub))
		{
			std::cerr << "BCrypt key generation failed.\n";
			return 1;
		}

		Store store;
		LoadStore(store);
		if (force)
			store = {};

		if (!FriendsCrypto::WriteFileBytes(privPath, priv) ||
			!FriendsCrypto::WriteFileBytes(Join(ToolDir(), "friends-public.bin"), pub) ||
			!WritePublicHeader(pub, store) ||
			!SaveStore(store))
		{
			std::cerr << "Failed to write key files.\n";
			return 1;
		}

		std::cout << "Wrote " << privPath << "\n";
		std::cout << "Wrote " << HeaderPath() << " (" << pub.size() << " byte public blob)\n";
		std::cout << "Rebuild HZ-Retail before using keys with a new binary.\n";
		return 0;
	}

	int CmdKey(int days, int count, const std::string& note)
	{
		if (count < 1)
			count = 1;

		std::vector<unsigned char> priv, pub;
		if (!LoadPrivate(priv) || !LoadPublic(pub))
		{
			std::cerr << "Run init first.\n";
			return 1;
		}

		Store store;
		LoadStore(store);

		for (int i = 0; i < count; ++i)
		{
			FriendsCrypto::Payload payload{};
			payload.version = FriendsCrypto::kPayloadVersion;
			payload.id = store.nextId++;
			payload.expDay = days <= 0 ? 0 : FriendsCrypto::Today() + static_cast<uint32_t>(days);

			unsigned char sig[FriendsCrypto::kSignatureSize]{};
			if (!FriendsCrypto::SignPayload(priv.data(), priv.size(), payload, sig))
			{
				std::cerr << "Sign failed for id " << payload.id << "\n";
				return 1;
			}

			KeyRecord rec;
			rec.id = payload.id;
			rec.expDay = payload.expDay;
			rec.note = note;
			rec.key = FriendsCrypto::FormatKey(payload, sig);
			store.keys.push_back(rec);
			std::cout << rec.key << "\n";
		}

		if (!SaveStore(store))
		{
			std::cerr << "Could not save friends-keys.json\n";
			return 1;
		}
		return 0;
	}

	int CmdList()
	{
		Store store;
		LoadStore(store);
		std::cout << "ID    EXPIRES     REVOKED  NOTE\n";
		for (const auto& k : store.keys)
		{
			char line[512]{};
			sprintf_s(line, "%-5u %-11s %-8s %s\n",
				k.id,
				FormatDay(k.expDay).c_str(),
				k.revoked ? "yes" : "no",
				k.note.c_str());
			std::cout << line;
		}
		if (store.keys.empty())
			std::cout << "(no keys)\n";
		return 0;
	}

	int CmdRevoke(uint32_t id)
	{
		if (id == 0)
		{
			std::cerr << "Usage: FriendsKeyGen revoke <id>\n";
			return 1;
		}

		std::vector<unsigned char> pub;
		if (!LoadPublic(pub))
		{
			std::cerr << "Run init first.\n";
			return 1;
		}

		Store store;
		LoadStore(store);
		bool found = false;
		for (auto& k : store.keys)
		{
			if (k.id == id)
			{
				k.revoked = true;
				found = true;
			}
		}
		if (!found)
		{
			std::cerr << "No key with id " << id << "\n";
			return 1;
		}

		if (!SaveStore(store) || !WritePublicHeader(pub, store))
			return 1;

		std::cout << "Revoked id " << id << ". Rebuild Trinity-Friends and ship an update for this to take effect.\n";
		return 0;
	}

	int CmdVerify(const std::string& key)
	{
		std::vector<unsigned char> pub;
		if (!LoadPublic(pub))
		{
			std::cerr << "Run init first.\n";
			return 1;
		}

		FriendsCrypto::Payload payload{};
		std::vector<unsigned char> sig;
		if (!FriendsCrypto::ParseKey(key, payload, sig))
		{
			std::cerr << "Invalid key format.\n";
			return 1;
		}

		const bool ok = FriendsCrypto::VerifyPayload(pub.data(), pub.size(), payload, sig.data(), sig.size());
		std::cout << (ok ? "VALID" : "INVALID")
			<< "  id=" << payload.id
			<< "  exp=" << FormatDay(payload.expDay)
			<< "  version=" << static_cast<unsigned>(payload.version) << "\n";
		return ok ? 0 : 1;
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		PrintUsage();
		return 1;
	}

	const std::string cmd = argv[1];
	if (cmd == "init")
	{
		bool force = false;
		for (int i = 2; i < argc; ++i)
			if (std::string(argv[i]) == "--force")
				force = true;
		return CmdInit(force);
	}
	if (cmd == "key")
	{
		int days = 0;
		int count = 1;
		std::string note;
		for (int i = 2; i < argc; ++i)
		{
			const std::string a = argv[i];
			if (a == "--days" && i + 1 < argc)
				days = atoi(argv[++i]);
			else if (a == "--count" && i + 1 < argc)
				count = atoi(argv[++i]);
			else if (a == "--note" && i + 1 < argc)
				note = argv[++i];
		}
		return CmdKey(days, count, note);
	}
	if (cmd == "list")
		return CmdList();
	if (cmd == "revoke")
		return CmdRevoke(argc > 2 ? static_cast<uint32_t>(strtoul(argv[2], nullptr, 10)) : 0);
	if (cmd == "verify")
		return CmdVerify(argc > 2 ? argv[2] : "");

	PrintUsage();
	return 1;
}
