#include "Fivem.hpp"
#include "../Definations/Variables.hpp"
#include "../FiveM-External.hpp"

#include <algorithm>
#include <cctype>
#include <climits>
#include <map>

namespace Cheat
{
	namespace
	{
		bool ValidRemotePtr(uintptr_t address)
		{
			return address >= 0x10000 && address <= 0x7FFFFFFFFFFFULL;
		}

		std::string ReadRemoteCString(uintptr_t address, size_t maxLen = 32)
		{
			if (!ValidRemotePtr(address) || maxLen == 0 || maxLen > 64)
				return {};

			std::string value = FrameWork::Memory::ReadProcessMemoryString(address, maxLen);
			while (!value.empty() && (value.back() == '\0' || (unsigned char)value.back() < 32))
				value.pop_back();
			return value;
		}

		std::string ReadMsvcString(uintptr_t objectAddr)
		{
			if (!ValidRemotePtr(objectAddr))
				return {};

			const uint64_t size = FrameWork::Memory::ReadMemory<uint64_t>(objectAddr + 16);
			const uint64_t capacity = FrameWork::Memory::ReadMemory<uint64_t>(objectAddr + 24);
			if (size == 0 || size > 64 || capacity < size || capacity > 0x1000)
				return {};

			const uintptr_t data = capacity < 16
				? objectAddr
				: FrameWork::Memory::ReadMemory<uintptr_t>(objectAddr);
			if (!ValidRemotePtr(data))
				return {};

			std::string value = FrameWork::Memory::ReadProcessMemoryString(data, static_cast<SIZE_T>(size + 1));
			if (value.size() > size)
				value.resize(static_cast<size_t>(size));
			return value;
		}

		bool LooksLikePlayerName(const std::string& name)
		{
			if (name.size() < 1 || name.size() > 32)
				return false;
			if (name == "** Invalid **" || name == "PkSD Teste" || name == "Unknown" || name == "NPC")
				return false;
			if (name.size() >= 7 && name.compare(0, 7, "player ") == 0)
				return false;

			for (unsigned char c : name)
			{
				if (c < 32 || c > 126)
					return false;
				if (c == '\\' || c == '/' || c == ':' || c == '<' || c == '>')
					return false;
			}
			return true;
		}

		int FeedNetIdFromIp(uint32_t ip)
		{
			if ((ip & 0xFFFF0000u) != 0xC0A80000u)
				return -1;
			return static_cast<int>((ip & 0xFFFFu) ^ 0xFEEDu);
		}

		std::string ReadNameAtNode(uintptr_t node)
		{
			static const int kStringOffs[] = { 0x18, 0x20, 0x28 };
			for (int off : kStringOffs)
			{
				std::string name = ReadMsvcString(node + off);
				if (LooksLikePlayerName(name))
					return name;
				name = ReadRemoteCString(node + off, 32);
				if (LooksLikePlayerName(name))
					return name;
			}
			return {};
		}

		bool LooksLikeNameMap(uintptr_t mapAddr)
		{
			if (!ValidRemotePtr(mapAddr) || (mapAddr & 7) != 0)
				return false;

			const uintptr_t head = FrameWork::Memory::ReadMemory<uintptr_t>(mapAddr);
			const int64_t size = FrameWork::Memory::ReadMemory<int64_t>(mapAddr + 8);
			const uintptr_t buckets = FrameWork::Memory::ReadMemory<uintptr_t>(mapAddr + 0x10);
			if (!ValidRemotePtr(head) || !ValidRemotePtr(buckets) || size < 0 || size > 2048)
				return false;

			const uintptr_t next = FrameWork::Memory::ReadMemory<uintptr_t>(head);
			const uintptr_t prev = FrameWork::Memory::ReadMemory<uintptr_t>(head + 8);
			if (!ValidRemotePtr(next) || !ValidRemotePtr(prev))
				return false;

			const uint64_t mask = FrameWork::Memory::ReadMemory<uint64_t>(mapAddr + 0x28);
			const bool maskOk = mask == 7 || mask == 15 || mask == 31 || mask == 63
				|| mask == 127 || mask == 255 || mask == 511 || mask == 1023
				|| mask == 2047 || mask == 4095;

			if (size == 0)
				return next == head && prev == head;

			if (maskOk)
				return true;

			return LooksLikePlayerName(ReadNameAtNode(next));
		}

		uintptr_t ResolveNameMapObject(uintptr_t namesPtr)
		{
			if (!namesPtr)
				return 0;

			const uintptr_t candidates[] = {
				namesPtr,
				namesPtr > 0x10 ? namesPtr - 0x10 : 0,
				FrameWork::Memory::ReadMemory<uintptr_t>(namesPtr),
			};

			for (uintptr_t candidate : candidates)
			{
				if (!candidate)
					continue;
				if (LooksLikeNameMap(candidate))
					return candidate;
				if (candidate > 0x10 && LooksLikeNameMap(candidate - 0x10))
					return candidate - 0x10;
			}
			return 0;
		}

		std::map<int, std::string> HarvestPlayerNameMap(uintptr_t namesPtr)
		{
			std::map<int, std::string> names;
			const uintptr_t mapAddr = ResolveNameMapObject(namesPtr);
			if (!mapAddr)
				return names;

			const uintptr_t head = FrameWork::Memory::ReadMemory<uintptr_t>(mapAddr);
			const int64_t size = FrameWork::Memory::ReadMemory<int64_t>(mapAddr + 8);
			if (!ValidRemotePtr(head) || size <= 0)
				return names;

			int walked = 0;
			for (uintptr_t cur = FrameWork::Memory::ReadMemory<uintptr_t>(head);
				cur && cur != head && ValidRemotePtr(cur) && walked < size + 8;
				cur = FrameWork::Memory::ReadMemory<uintptr_t>(cur), ++walked)
			{
				const std::string name = ReadNameAtNode(cur);
				if (name.empty())
					continue;

				const int id32 = FrameWork::Memory::ReadMemory<int>(cur + 0x10);
				if (id32 >= 0 && id32 <= 65535)
					names[id32] = name;
			}

			return names;
		}

		uintptr_t LocatePlayerNamesMap(uintptr_t moduleBase, uint64_t moduleSize)
		{
			if (!moduleBase)
				return 0;

			const uint64_t scanSize = moduleSize ? moduleSize : 0x200000ull;
			const std::vector<uint8_t> signatures[] = {
				{ 0x48, 0x8B, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC1, 0xE1 },
				{ 0x48, 0x8D, 0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC1, 0xE1 },
				{ 0x48, 0x8B, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x48, 0xC1, 0xE1 },
			};

			for (const auto& signature : signatures)
			{
				uint64_t searchAt = moduleBase;
				for (int attempt = 0; attempt < 24 && searchAt < moduleBase + scanSize; ++attempt)
				{
					const uint64_t hit = (attempt == 0)
						? FrameWork::Memory::FindSignature(signature, moduleBase, scanSize)
						: FrameWork::Memory::FindSignatureFrom(signature, searchAt, moduleBase, scanSize);
					if (!hit)
						break;

					const uintptr_t ptr = FrameWork::Memory::ResolveRelativeAddress(hit, 3, 7);
					if (ResolveNameMapObject(ptr))
						return ptr;

					searchAt = hit + 1;
				}
			}

			return 0;
		}

		std::string ReadNameFromPlayerInfo(CPlayerInfo* info)
		{
			if (!info)
				return {};

			const uintptr_t base = reinterpret_cast<uintptr_t>(info);
			static const uint64_t kObjectOffs[] = { 0x20, 0x28, 0x40, 0x48, 0x80, 0xA0, 0xB0, 0xC0, 0xF0, 0xF8 };
			for (uint64_t off : kObjectOffs)
			{
				std::string name = ReadMsvcString(base + off);
				if (LooksLikePlayerName(name))
					return name;

				const uintptr_t ptr = FrameWork::Memory::ReadMemory<uintptr_t>(base + off);
				if (!ValidRemotePtr(ptr))
					continue;

				name = ReadMsvcString(ptr);
				if (LooksLikePlayerName(name))
					return name;
				name = ReadRemoteCString(ptr, 32);
				if (LooksLikePlayerName(name))
					return name;

				static const uint64_t kInnerOffs[] = { 0x84, 0xA4, 0xB4, 0xD8, 0xE0, 0xF0 };
				for (uint64_t inner : kInnerOffs)
				{
					name = ReadRemoteCString(ptr + inner, 32);
					if (LooksLikePlayerName(name))
						return name;
					name = ReadMsvcString(ptr + inner);
					if (LooksLikePlayerName(name))
						return name;
				}
			}

			static const uint64_t kCStringOffs[] = { 0x7C, 0x84, 0xA4, 0xB4, 0xBC, 0xE0, 0xF0, 0xFC, 0x10C };
			for (uint64_t off : kCStringOffs)
			{
				const std::string name = ReadRemoteCString(base + off, 32);
				if (LooksLikePlayerName(name))
					return name;
			}

			return {};
		}

		int ScanEncodedServerId(uintptr_t block, const std::map<int, std::string>& names)
		{
			if (!ValidRemotePtr(block))
				return -1;

			int fallback = -1;
			for (int off = 0; off <= 0x1C0; off += 4)
			{
				const uint32_t ip = FrameWork::Memory::ReadMemory<uint32_t>(block + off);
				const int id = FeedNetIdFromIp(ip);
				if (id < 0 || id > 65535)
					continue;
				if (!names.empty() && names.find(id) != names.end())
					return id;
				if (fallback < 0 && id >= 0 && id < 2048)
					fallback = id;
			}
			return fallback;
		}

		int ReadPedNetId(CPlayerInfo* info, const std::map<int, std::string>& names)
		{
			if (!info)
				return -1;

			const uintptr_t base = reinterpret_cast<uintptr_t>(info);
			static int cachedOffset = -1;

			const int encoded = ScanEncodedServerId(base, names);
			if (encoded >= 0 && (names.empty() || names.find(encoded) != names.end()))
				return encoded;

			const uintptr_t nested = FrameWork::Memory::ReadMemory<uintptr_t>(base + 0x20);
			if (nested != base)
			{
				const int nestedId = ScanEncodedServerId(nested, names);
				if (nestedId >= 0 && (names.empty() || names.find(nestedId) != names.end()))
					return nestedId;
			}

			auto tryOffset = [&](int offset) -> int
			{
				if (offset <= 0)
					return INT_MIN;
				const uint16_t id16 = FrameWork::Memory::ReadMemory<uint16_t>(base + offset);
				const int id32 = FrameWork::Memory::ReadMemory<int>(base + offset);
				if (!names.empty())
				{
					if (names.find(static_cast<int>(id16)) != names.end())
						return id16;
					if (id32 >= 0 && id32 <= 65535 && names.find(id32) != names.end())
						return id32;
				}
				if (id16 != 0 && id16 < 2048)
					return id16;
				if (id32 >= 0 && id32 < 2048)
					return id32;
				return INT_MIN;
			};

			if (cachedOffset >= 0)
			{
				const int value = tryOffset(cachedOffset);
				if (value != INT_MIN && (names.empty() || names.find(value) != names.end()))
					return value;
			}

			const int candidates[] = {
				static_cast<int>(Offsets::PlayerNetID), 0xE8, 0x88, 0xE0, 0xEC, 0x90, 0x80, 0xA0
			};
			for (int offset : candidates)
			{
				const int value = tryOffset(offset);
				if (value == INT_MIN)
					continue;
				if (!names.empty() && names.find(value) == names.end())
					continue;
				cachedOffset = offset;
				Offsets::PlayerNetID = offset;
				return value;
			}

			if (encoded >= 0)
				return encoded;

			for (int offset : candidates)
			{
				const int value = tryOffset(offset);
				if (value != INT_MIN)
					return value;
			}

			return FrameWork::Memory::ReadMemory<int>(info + Offsets::PlayerNetID);
		}
	}
#if defined(TRINITY_DEV) && TRINITY_DEV
	static uint64_t DiscoverVehiclePlateTextOffset(uint64_t moduleBase, int gameVersion)
	{
		if (!moduleBase)
			return 0;

		uint64_t getPlateRva = 0;
		uint64_t setPlateRva = 0;

		if (gameVersion >= 2802)
		{
			getPlateRva = 0xD64774;
			setPlateRva = 0xD6B378;
		}

		std::map<int32_t, int> displacementVotes;

		auto scanHandler = [&](uint64_t rva)
		{
			if (!rva)
				return;

			const auto bytes = FrameWork::Memory::ReadBytes(moduleBase + rva, 0x500);
			for (size_t i = 0; i + 7 < bytes.size(); ++i)
			{
				const uint8_t rex = bytes[i];
				const uint8_t opcode = bytes[i + 1];
				if (rex != 0x48 || (opcode != 0x8D && opcode != 0x8B && opcode != 0x89))
					continue;

				const uint8_t modrm = bytes[i + 2];
				const uint8_t mod = (modrm >> 6) & 0x3;
				if (mod != 0x2)
					continue;

				int32_t displacement = 0;
				std::memcpy(&displacement, bytes.data() + i + 3, sizeof(displacement));
				if (displacement >= 0x800 && displacement <= 0x1800)
					++displacementVotes[displacement];
			}
		};

		scanHandler(getPlateRva);
		scanHandler(setPlateRva);

		int bestVotes = 0;
		uint64_t bestOffset = 0;
		for (const auto& entry : displacementVotes)
		{
			if (entry.second > bestVotes)
			{
				bestVotes = entry.second;
				bestOffset = static_cast<uint64_t>(entry.first);
			}
		}

		return bestOffset;
	}
#endif

	void FivemSDK::Intialize()
	{
		if (bIsIntialized)
			return;

		static const std::vector<uint8_t> CheckTable1 = { 0x48, 0xB8, 0x01, 0x00, 0x00 };
		static const std::vector<uint8_t> CheckTable2 = { 0x48, 0x89, 0x5C, 0x24, 0x10 };

		std::vector<std::wstring> ProcessList =
		{
			XorStr(L"FiveM_b1604_GameProcess.exe"),
			XorStr(L"FiveM_b1604_GTAProcess.exe"),
			XorStr(L"FiveM_b2060_GameProcess.exe"),
			XorStr(L"FiveM_b2060_GTAProcess.exe"),
			XorStr(L"FiveM_b2189_GameProcess.exe"),
			XorStr(L"FiveM_b2189_GTAProcess.exe"),
			XorStr(L"FiveM_b2372_GameProcess.exe"),
			XorStr(L"FiveM_b2372_GTAProcess.exe"),
			XorStr(L"FiveM_b2545_GameProcess.exe"),
			XorStr(L"FiveM_b2545_GTAProcess.exe"),
			XorStr(L"FiveM_b2612_GameProcess.exe"),
			XorStr(L"FiveM_b2612_GTAProcess.exe"),
			XorStr(L"FiveM_b2699_GameProcess.exe"),
			XorStr(L"FiveM_b2699_GTAProcess.exe"),
			XorStr(L"FiveM_b2802_GameProcess.exe"),
			XorStr(L"FiveM_b2802_GTAProcess.exe"),
			XorStr(L"FiveM_b2944_GameProcess.exe"),
			XorStr(L"FiveM_b2944_GTAProcess.exe"),
			XorStr(L"FiveM_b3095_GameProcess.exe"),
			XorStr(L"FiveM_b3095_GTAProcess.exe"),
			XorStr(L"FiveM_b3258_GameProcess.exe"),
			XorStr(L"FiveM_b3258_GTAProcess.exe"),
			XorStr(L"FiveM_b3323_GameProcess.exe"),
			XorStr(L"FiveM_b3323_GTAProcess.exe"),
			XorStr(L"FiveM_b3407_GameProcess.exe"),
			XorStr(L"FiveM_b3407_GTAProcess.exe"),
			XorStr(L"FiveM_b3570_GameProcess.exe"),
			XorStr(L"FiveM_b3570_GTAProcess.exe"),
			XorStr(L"FiveM_GameProcess.exe"),
			XorStr(L"FiveM_GTAProcess.exe"),
		};

		DWORD fallbackPid = 0;
		std::wstring fallbackModule;
		std::wstring selectedModule;

		for (size_t i = 0; i < ProcessList.size(); i++)
		{
			const std::wstring& processName = ProcessList.at(i);
			DWORD foundPid = FrameWork::Memory::GetProcessPidByName(processName.c_str());
			if (!foundPid)
				continue;

			const bool isGtaProcess = processName.find(L"GTAProcess.exe") != std::wstring::npos;
			if (isGtaProcess)
			{
				Pid = foundPid;
				selectedModule = processName;
				break;
			}

			if (!fallbackPid)
			{
				fallbackPid = foundPid;
				fallbackModule = processName;
			}
		}

		if (!Pid && fallbackPid)
		{
			Pid = fallbackPid;
			selectedModule = fallbackModule;
		}

		if (!Pid)
			FrameWork::Memory::FindFiveMGameProcess(Pid, selectedModule);

		if (Pid)
		{
			ModuleName = FrameWork::Misc::Wstring2String(selectedModule);
			ModuleBase = FrameWork::Memory::GetModuleBaseByName(Pid, selectedModule.c_str());

			std::regex Regex(XorStr(R"_(FiveM_b(\d+))_"));
			std::smatch Match;

			if (std::regex_search(ModuleName, Match, Regex)) {
				GameVersion = std::stoi(Match[1].str());
			}
			else {
				GameVersion = 0;
			}

			FrameWork::Memory::AttachProces(Pid);
		}

		std::vector<std::pair<uint64_t, int>> VersionsMap =
		{
			{ 0x6E1330, 1604 },
			{ 0x6E5630, 2060 },
			{ 0x6EA5A0, 2189 },
			{ 0x6F11C0, 2372 },
			{ 0x6F4D20, 2545 },
			{ 0x6F7990, 2612 },
			{ 0x6FBCE0, 2699 },
			{ 0x6FFE78, 2802 },
			{ 0x7014E0, 2944 },
			{ 0x7072C0, 3095 },
			{ 0x70A064, 3258 },
			{ 0x70A814, 3323 },
			{ 0x711AC0, 3407 },
			{ 0x7145D0, 3570 },
		};

		for (size_t i = 0; i < VersionsMap.size(); i++)
		{
			std::vector<uint8_t> ReadBuffer(CheckTable1.size());

			int Checks1 = 0;
			int Checks2 = 0;

			for (size_t x = 0; x < CheckTable1.size(); x++)
			{
				uint8_t Byte = FrameWork::Memory::ReadMemory<uint8_t>(ModuleBase + VersionsMap.at(i).first + x);
				if (Byte == CheckTable1[x])
					Checks1++;

				if (Byte == CheckTable2[x])
					Checks2++;
			}

			if (Checks1 >= CheckTable1.size() - 1 || Checks2 >= CheckTable2.size() - 1)
			{
				RealGameVersion = VersionsMap.at(i).second;
				break;
			}
		}

		if (RealGameVersion == 0)
		{
			RealGameVersion = GameVersion;
		}

		if (RealGameVersion == 1604)
		{
			World = ModuleBase + 0x24C8858;
			ReplayInterface = ModuleBase + 0x1F033A8;
			Camera = ModuleBase + 0x1FB3CD8;
			ViewPort = ModuleBase + 0x1F9EBE0;

			PlayerAimingAt = ModuleBase + 0x1FADF50;
			HandleBullet = ModuleBase + 0xF15F3C;

			CanCombatRoll = ModuleBase + 0x6E1330;
			GameplayCamHolder = ModuleBase + 0x280540;
			GameplayCamTarget = ModuleBase + 0x1FB49A0;
			BlipList = ModuleBase + 0x1FB4DE0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x10B8;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10C8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10D8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x2A0;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x14E0;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2060)
		{
			World = ModuleBase + 0x24E6D90;
			ReplayInterface = ModuleBase + 0x1F2E7A8;
			Camera = ModuleBase + 0x1FC7E38;
			ViewPort = ModuleBase + 0x1FC6E00;

			PlayerAimingAt = ModuleBase + 0x1FD5C20;
			HandleBullet = ModuleBase + 0xF3C010;

			CanCombatRoll = ModuleBase + 0x6E5630;
			GameplayCamHolder = ModuleBase + 0x282360;
			GameplayCamTarget = ModuleBase + 0x1FC8B00;
			BlipList = ModuleBase + 0x1FC8F40;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x10B8;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10C8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10D8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x2A0;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x14E0;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2189)
		{
			World = ModuleBase + 0x24E6E28;
			ReplayInterface = ModuleBase + 0x1F2E7A8;
			Camera = ModuleBase + 0x1FCB668;
			ViewPort = ModuleBase + 0x1FCA630;

			PlayerAimingAt = ModuleBase + 0x1FD9450;
			HandleBullet = ModuleBase + 0xF47520;

			CanCombatRoll = ModuleBase + 0x6EA5A0;
			GameplayCamHolder = ModuleBase + 0x283180;
			GameplayCamTarget = ModuleBase + 0x1FCC330;
			BlipList = ModuleBase + 0x1FCC770;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x10B8;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10C8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10D8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x2A0;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x14E0;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2372)
		{
			World = ModuleBase + 0x252D4A8;
			ReplayInterface = ModuleBase + 0x1F5B820;
			Camera = ModuleBase + 0x1FBCFA8;
			ViewPort = ModuleBase + 0x1FBC100;

			PlayerAimingAt = ModuleBase + 0x1FCA160;
			HandleBullet = ModuleBase + 0xFF716C;

			CanCombatRoll = ModuleBase + 0x6F11C0;
			GameplayCamHolder = ModuleBase + 0x284790;
			GameplayCamTarget = ModuleBase + 0x1FBD298;
			BlipList = ModuleBase + 0x1FBD6E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x10B8;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10C8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10D8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x2A0;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x14E0;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2545)
		{
			World = ModuleBase + 0x254D448;
			ReplayInterface = ModuleBase + 0x1F5B820;
			Camera = ModuleBase + 0x1FBCFA8;
			ViewPort = ModuleBase + 0x1FBC100;

			PlayerAimingAt = ModuleBase + 0x1FCA160;
			HandleBullet = ModuleBase + 0xFF1B40;

			CanCombatRoll = ModuleBase + 0x6F4D20;
			GameplayCamHolder = ModuleBase + 0x286590;
			GameplayCamTarget = ModuleBase + 0x1FBD298;
			BlipList = ModuleBase + 0x1FBD6E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2612)
		{
			World = ModuleBase + 0x254D448;
			ReplayInterface = ModuleBase + 0x1F5B820;
			Camera = ModuleBase + 0x1FBCFA8;
			ViewPort = ModuleBase + 0x1FBC100;

			PlayerAimingAt = ModuleBase + 0x1FCA160;
			HandleBullet = ModuleBase + 0xFF716C;

			CanCombatRoll = ModuleBase + 0x6F7990;
			GameplayCamHolder = ModuleBase + 0x287110;
			GameplayCamTarget = ModuleBase + 0x1FBD298;
			BlipList = ModuleBase + 0x1FBD6E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2699)
		{
			World = ModuleBase + 0x254D448;
			ReplayInterface = ModuleBase + 0x1F5B820;
			Camera = ModuleBase + 0x1FBCFA8;
			ViewPort = ModuleBase + 0x1FBC100;

			PlayerAimingAt = ModuleBase + 0x1FCA160;
			HandleBullet = ModuleBase + 0xFF716C;

			CanCombatRoll = ModuleBase + 0x6FBCE0;
			GameplayCamHolder = ModuleBase + 0x288120;
			GameplayCamTarget = ModuleBase + 0x1FBD298;
			BlipList = ModuleBase + 0x1FBD6E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2802)
		{
			World = ModuleBase + 0x254D448;
			ReplayInterface = ModuleBase + 0x1F5B820;
			Camera = ModuleBase + 0x1FBCFA8;
			ViewPort = ModuleBase + 0x1FBC100;

			PlayerAimingAt = ModuleBase + 0x1FCA160;
			HandleBullet = ModuleBase + 0xFF716C;

			CanCombatRoll = ModuleBase + 0x6FFE78;
			GameplayCamHolder = ModuleBase + 0x288D72;
			GameplayCamTarget = ModuleBase + 0x1FBD298;
			BlipList = ModuleBase + 0x1FBD6E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0x88;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xCF0;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 2944)
		{
			World = ModuleBase + 0x257BEA0;
			ReplayInterface = ModuleBase + 0x1F42068;
			Camera = ModuleBase + 0x1FEB968;
			ViewPort = ModuleBase + 0x1FEAAC0;

			CanCombatRoll = ModuleBase + 0x7014E0;
			GameplayCamHolder = ModuleBase + 0x28A916;
			GameplayCamTarget = ModuleBase + 0x1FEBC58;
			BlipList = ModuleBase + 0x1121F0;

			PlayerAimingAt = ModuleBase + 0x1FF8AF0;
			HandleBullet = ModuleBase + 0x1003F80;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::Armor = 0x150C;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD10;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 3095)
		{
			World = ModuleBase + 0x2593320;
			ReplayInterface = ModuleBase + 0x1F58B58;
			Camera = ModuleBase + 0x2002888;
			ViewPort = ModuleBase + 0x20019E0;

			PlayerAimingAt = ModuleBase + 0x200FA10;
			HandleBullet = ModuleBase + 0x100F5A4;
			Offsets::last = 0xD10;
			Invisible = ModuleBase + 0x1386426;
			CanCombatRoll = ModuleBase + 0x7072C0;
			GameplayCamHolder = ModuleBase + 0x28E5DA;
			GameplayCamTarget = ModuleBase + 0x2002B78;
			BlipList = ModuleBase + 0x2002FA0;

			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::WeaponManager = 0x10B8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD60;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 3258)
		{
			World = ModuleBase + 0x25B14B0;
			ReplayInterface = ModuleBase + 0x1FBD4F0;
			Camera = ModuleBase + 0x201ED50;
			ViewPort = ModuleBase + 0x201DBA0;

			PlayerAimingAt = ModuleBase + 0X202C8D0;
			HandleBullet = ModuleBase + 0x101A5F4;

			Invisible = ModuleBase + 0x119345A;
			CanCombatRoll = ModuleBase + 0x70A064;
			GameplayCamHolder = ModuleBase + 0x29029A;
			GameplayCamTarget = ModuleBase + 0x201ED90;
			BlipList = ModuleBase + 0x2023400;

			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::WeaponManager = 0x10B8;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD60;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 3323)
		{
			World = ModuleBase + 0x25C15B0;
			ReplayInterface = ModuleBase + 0x1F85458;
			Camera = ModuleBase + 0x202EB48;
			ViewPort = ModuleBase + 0x202DC50;

			PlayerAimingAt = ModuleBase + 0X203C970;
			HandleBullet = ModuleBase + 0x1026CB0;

			Invisible = ModuleBase + 0x11A174C;
			CanCombatRoll = ModuleBase + 0x70A814;
			GameplayCamHolder = ModuleBase + 0x290274;
			GameplayCamTarget = ModuleBase + 0x202EE38;
			BlipList = ModuleBase + 0x2CEDFC;

			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::WeaponManager = 0x10B8;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD60;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 3407)
		{
			World = ModuleBase + 0x25D7108;
			ReplayInterface = ModuleBase + 0x1F9A9D8;
			Camera = ModuleBase + 0x20440C8;
			ViewPort = ModuleBase + 0x20431C0;

			PlayerAimingAt = ModuleBase + 0x2051EE0;
			HandleBullet = ModuleBase + 0x102FF8C;

			Invisible = ModuleBase + 0x11B1A2C;
			CanCombatRoll = ModuleBase + 0x711AC0;
			GameplayCamHolder = ModuleBase + 0x290804;
			GameplayCamTarget = ModuleBase + 0x20443B8;
			BlipList = ModuleBase + 0x2CF3E0;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::DoorLock = 0x13C0;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::DoorLock = 0x13C0;
			Offsets::GodMode = 0xD60;

			bIsIntialized = true;
		}
		else if (RealGameVersion == 3570)
		{
			World = ModuleBase + 0x25ec580;
			ReplayInterface = ModuleBase + 0x1fb0418;
			Camera = ModuleBase + 0x2059a48;
			ViewPort = ModuleBase + 0x2058ba0;

			PlayerAimingAt = ModuleBase + 0x2067860;
			HandleBullet = ModuleBase + 0x102D550;

			Invisible = ModuleBase + 0x11C2B5C;
			CanCombatRoll = ModuleBase + 0x7145D0;
			GameplayCamHolder = ModuleBase + 0x291410;
			GameplayCamTarget = ModuleBase + 0x2058A18;
			BlipList = ModuleBase + 0x2061870;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::DoorLock = 0x13C0;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::GodMode = 0xD60;

			bIsIntialized = true;
		}
		else if (Pid && ModuleBase)
		{
			World = ModuleBase + 0x25ec580;
			ReplayInterface = ModuleBase + 0x1fb0418;
			Camera = ModuleBase + 0x2059a48;
			ViewPort = ModuleBase + 0x2058ba0;
			PlayerAimingAt = ModuleBase + 0x2067860;
			HandleBullet = ModuleBase + 0x102D550;
			Invisible = ModuleBase + 0x11C2B5C;
			CanCombatRoll = ModuleBase + 0x7145D0;
			GameplayCamHolder = ModuleBase + 0x291410;
			GameplayCamTarget = ModuleBase + 0x2058A18;
			BlipList = ModuleBase + 0x2061870;
			Offsets::last = 0xD10;
			Offsets::EntityType = 0x1098;
			Offsets::FragInsNmGTA = 0x1430;
			Offsets::ConfigFlags = 0x1444;
			Offsets::PlayerInfo = 0x10A8;
			Offsets::PlayerNetID = 0xE8;
			Offsets::DoorLock = 0x13C0;
			Offsets::WeaponManager = 0x10B8;
			Offsets::VisibleFlag = 0x145C;
			Offsets::MaxHealth = 0x284;
			Offsets::SeatBelt = 0x143C;
			Offsets::SeatBeltWindShield = Offsets::SeatBelt + 12;
			Offsets::Armor = 0x150C;
			Offsets::SpeedModifier = 0xD40;
			Offsets::GodMode = 0xD60;
			if (!RealGameVersion)
				RealGameVersion = GameVersion ? GameVersion : 3570;
			bIsIntialized = true;
		}

		if (bIsIntialized) {
			Offsets::VehiclePlatePointerOnly = (RealGameVersion >= 2802);
			if (RealGameVersion >= 2802)
			{
				Offsets::CurrentVehicle = 0xD10;
				Offsets::LastVehicle = 0xD10;
				Offsets::VehiclePlateText = 0;
				Offsets::VehiclePlateShader = 0x138;
			}
			else if (RealGameVersion >= 2372)
			{
				Offsets::CurrentVehicle = 0xD30;
				Offsets::LastVehicle = 0xD30;
				Offsets::VehiclePlateText = 0x928;
				Offsets::VehiclePlateShader = 0x130;
			}
			else
			{
				Offsets::CurrentVehicle = 0xD30;
				Offsets::LastVehicle = 0xD30;
				Offsets::VehiclePlateText = 0x928;
				Offsets::VehiclePlateShader = 0x130;
			}

			if (!Offsets::VehiclePlatePointerOnly)
				Offsets::VehiclePlateText = 0;

			FrameWork::Memory::AttachProces(Pid);

			MODULEINFO mi = {};
			if (GetModuleInformation(FrameWork::Memory::AttachedProcessHandle, (HMODULE)ModuleBase, &mi, sizeof(mi)))
				ModuleBaseSize = mi.SizeOfImage;
			else
				ModuleBaseSize = 0x10000000;
			FrameWork::Memory::SetModuleInfo(ModuleBase, ModuleBaseSize);

#if defined(TRINITY_DEV) && TRINITY_DEV
			if (!Offsets::VehiclePlateText)
			{
				const uint64_t discoveredOffset = DiscoverVehiclePlateTextOffset(ModuleBase, RealGameVersion);
				if (discoveredOffset)
					Offsets::VehiclePlateText = discoveredOffset;
			}
#endif
		}
		else {
			std::cout << "[ERROR] Game version not supported or failed to apply offsets!" << std::endl;
		}

		// std::cout << "[DEBUG] Finding citizen-playernames-five.dll..." << std::endl;
		CitizemPlayerNamesModule = FrameWork::Memory::GetModuleBaseByName(Pid, XorStr(L"citizen-playernames-five.dll"));
		const uint64_t playerNamesSize = FrameWork::Memory::GetModuleSizeByName(Pid, XorStr(L"citizen-playernames-five.dll"));
		NetIdToNamesPtr = LocatePlayerNamesMap(CitizemPlayerNamesModule, playerNamesSize);
		std::cout << "[DEBUG] NetIdToNamesPtr: 0x" << std::hex << NetIdToNamesPtr << std::dec << std::endl;

		HandleBulletMagic = FrameWork::Memory::FindSignature({ 0x48, 0x8D, 0x45, 0x00, 0xF3, 0x0F, 0x10, 0x00, 0xF3, 0x0F, 0x10, 0x48, 0x00, 0xF3, 0x0F, 0x11, 0x45 }, ModuleBase, ModuleBaseSize);
		std::cout << "[DEBUG] HandleBulletMagic (SilentAim): 0x" << std::hex << HandleBulletMagic << std::dec << std::endl;

		{
			static const uint8_t kHandleBulletPattern[] =
			{
				0xF3, 0x41, 0x0F, 0x10, 0x19,
				0xF3, 0x41, 0x0F, 0x10, 0x41, 0x04,
				0xF3, 0x41, 0x0F, 0x10, 0x51, 0x08
			};

			auto validateHandleBullet = [&](uint64_t address) -> bool
			{
				if (!address)
					return false;

				for (size_t i = 0; i < sizeof(kHandleBulletPattern); ++i)
				{
					if (FrameWork::Memory::ReadMemory<uint8_t>(address + i) != kHandleBulletPattern[i])
						return false;
				}
				return true;
			};

			auto tryHandleBulletCandidates = [&](const std::vector<uint64_t>& candidates) -> uint64_t
			{
				for (uint64_t candidate : candidates)
				{
					if (validateHandleBullet(candidate))
						return candidate;
				}
				return 0;
			};

			const uint64_t preferredHandleBullet = HandleBullet;

			if (!validateHandleBullet(HandleBullet))
			{
				std::vector<uint64_t> candidates;
				switch (RealGameVersion)
				{
				case 3258:
					candidates = { ModuleBase + 0x101A5F4, ModuleBase + 0x100F5A4 };
					break;
				case 3095:
					candidates = { ModuleBase + 0x100F5A4 };
					break;
				case 3323:
					candidates = { ModuleBase + 0x1026CB0 };
					break;
				case 3407:
					candidates = { ModuleBase + 0x102FF8C };
					break;
				case 3570:
					candidates = { ModuleBase + 0x102D550 };
					break;
				case 2944:
					candidates = { ModuleBase + 0x1003F80 };
					break;
				case 2545:
					candidates = { ModuleBase + 0xFF1B40, ModuleBase + 0xFF716C };
					break;
				default:
					break;
				}

				HandleBullet = tryHandleBulletCandidates(candidates);
			}

			if (!validateHandleBullet(HandleBullet))
			{
				const std::vector<uint8_t> handleBulletSig =
				{
					0xF3, 0x41, 0x0F, 0x10, 0x19,
					0xF3, 0x41, 0x0F, 0x10, 0x41, 0x04,
					0xF3, 0x41, 0x0F, 0x10, 0x51, 0x08
				};

				uint64_t searchAt = ModuleBase;
				uint64_t bestMatch = 0;
				uint64_t bestDistance = UINT64_MAX;

				while (searchAt && searchAt < ModuleBase + ModuleBaseSize)
				{
					const uint64_t hit = FrameWork::Memory::FindSignatureFrom(handleBulletSig, searchAt, ModuleBase, ModuleBaseSize);
					if (!hit)
						break;

					if (validateHandleBullet(hit))
					{
						const uint64_t distance = preferredHandleBullet > hit
							? preferredHandleBullet - hit
							: hit - preferredHandleBullet;

						if (distance < bestDistance)
						{
							bestDistance = distance;
							bestMatch = hit;
						}
					}

					searchAt = hit + 1;
				}

				HandleBullet = bestMatch;
			}
		}
		std::cout << "[DEBUG] HandleBullet: 0x" << std::hex << HandleBullet << std::dec << std::endl;
		if (!HandleBullet)
			std::cout << "[ERROR] HandleBullet not found for build " << RealGameVersion << std::endl;

		Offsets::MagicBulletsPatch = FrameWork::Memory::FindSignature({ 0x0F, 0x29, 0x4F, 0x00, 0x83, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x4F }, ModuleBase, ModuleBaseSize);
		std::cout << "[DEBUG] MagicBulletsPatch: 0x" << std::hex << Offsets::MagicBulletsPatch << std::dec << std::endl;

		uintptr_t ObjectSigAddr = FrameWork::Memory::FindSignature({ 0x4c, 0x8b, 0x41, 0x00, 0x4d, 0x85, 0xc9 }, ModuleBase, ModuleBaseSize);
		if (ObjectSigAddr)
			Offsets::CObject = FrameWork::Memory::ReadMemory<uint8_t>(ObjectSigAddr + 3);
		std::cout << "[DEBUG] CObject offset: 0x" << std::hex << (int)Offsets::CObject << std::dec << std::endl;

		uintptr_t CWeaponSigAddr = FrameWork::Memory::FindSignature({ 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0xd7, 0xe8 }, ModuleBase, ModuleBaseSize);
		if (CWeaponSigAddr)
			Offsets::CWeapon = FrameWork::Memory::ReadMemory<int>(CWeaponSigAddr + 3);
		std::cout << "[DEBUG] CWeapon offset: 0x" << std::hex << Offsets::CWeapon << std::dec << std::endl;
		// Vehicle Offsets Scanning
		uintptr_t VehicleEngineSigAddr = FrameWork::Memory::FindSignature({ 0xf3, 0x0f, 0x11, 0x80, 0x00, 0x00, 0x00, 0x00, 0x48, 0x8b, 0x87, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x05 }, ModuleBase, ModuleBaseSize);
		if (VehicleEngineSigAddr)
			Offsets::VehicleEngineHealth = FrameWork::Memory::ReadMemory<int>(VehicleEngineSigAddr + 4);

	
	}


	bool FivemSDK::UpdateEntities()
	{
		if (!pWorld)
		{
			pWorld = (CWorld*)FrameWork::Memory::ReadMemory<uint64_t>(World);

			if (!pWorld)
				return false;
		}

		pLocalPlayer = pWorld->LocalPlayer();

		if (pReplayInterface && !pLocalPlayer)
		{
			return false;
		}

		if (!pLocalPlayer)
			return false;

		if (!pReplayInterface)
		{
			pReplayInterface = (CReplayInterface*)FrameWork::Memory::ReadMemory<uint64_t>(ReplayInterface);

			if (!pReplayInterface)
				return false;
		}

		if (!pPedInterface)
		{
			pPedInterface = pReplayInterface->PedInterface();

			if (!pPedInterface)
				return false;
		}

		if (!pCamGameplayDirector)
		{
			pCamGameplayDirector = (CCamGameplayDirector*)FrameWork::Memory::ReadMemory<uint64_t>(Camera);

			if (!pCamGameplayDirector)
				return false;
		}

		LockLists.lock();

		std::map<int, std::string> namesCopy;
		{
			std::lock_guard<std::mutex> namesLock(LockPlayerList);
			namesCopy = PlayerNamesMap;
		}

		EntityList.clear();
		EntityList.shrink_to_fit();

		for (size_t i = 0; i < pPedInterface->PedMaximum(); i++)
		{
			CPed* Ped = pPedInterface->PedList()->Ped(i);
			if (!Ped)
				continue;

			if (g_Options.Visuals.Players.ExcludeDeads && Ped->GetHealth() <= 101)
				continue;

			CPlayerInfo* playerInfo = Ped->GetPlayerInfo();

			PedStaticInfo StaticInfo;
			{
				StaticInfo.Ped = Ped;
				StaticInfo.iIndex = i;
				StaticInfo.bIsLocalPlayer = (Ped == pLocalPlayer);
				StaticInfo.bIsNPC = Ped->IsNPC();
				StaticInfo.VisibleCheck = Ped->IsVisible();
				StaticInfo.IsFriend = false;
				StaticInfo.NetId = -1;
				StaticInfo.Name.clear();

				if (playerInfo)
				{
					StaticInfo.NetId = ReadPedNetId(playerInfo, namesCopy);
					if (FriendList.find(StaticInfo.NetId) != FriendList.end())
						StaticInfo.IsFriend = true;
				}

				if (!StaticInfo.bIsNPC)
				{
					if (StaticInfo.NetId >= 0)
					{
						auto it = namesCopy.find(StaticInfo.NetId);
						if (it != namesCopy.end() && LooksLikePlayerName(it->second))
							StaticInfo.Name = it->second;
					}

					if (!LooksLikePlayerName(StaticInfo.Name) && playerInfo)
					{
						StaticInfo.Name = ReadNameFromPlayerInfo(playerInfo);
						if (!LooksLikePlayerName(StaticInfo.Name))
							StaticInfo.Name = g_Fivem.GetPlayerName(reinterpret_cast<uint64_t>(playerInfo) + 0x20, StaticInfo.NetId);
					}
				}
				else
				{
					StaticInfo.NetId = -1;
					StaticInfo.Name = XorStr("NPC");
				}
			}

			Entity CurrentEntity;
			CurrentEntity.StaticInfo = StaticInfo;

			if (!StaticInfo.bIsNPC)
			{
				if (LooksLikePlayerName(StaticInfo.Name))
					CurrentEntity.NetworkInfo.UserName = StaticInfo.Name;
				else if (StaticInfo.NetId >= 0)
				{
					auto it = namesCopy.find(StaticInfo.NetId);
					if (it != namesCopy.end() && LooksLikePlayerName(it->second))
						CurrentEntity.NetworkInfo.UserName = it->second;
				}

				if (!LooksLikePlayerName(CurrentEntity.NetworkInfo.UserName))
					CurrentEntity.NetworkInfo.UserName = XorStr("Unknown");
				else
					StaticInfo.Name = CurrentEntity.NetworkInfo.UserName;
			}
			else
			{
				CurrentEntity.NetworkInfo.UserName = XorStr("NPC");
			}

			CurrentEntity.StaticInfo.Name = StaticInfo.Name;

			CurrentEntity.Cordinates = Ped->GetCoordinate();
			CurrentEntity.Visible = Ped->IsVisible();

			CurrentEntity.HeadPos = WorldToScreen(GetBonePosVec3(CurrentEntity, SKEL_Head));

			if (CurrentEntity.StaticInfo.bIsLocalPlayer)
			{
				LocalPlayerInfo.WorldPos = CurrentEntity.Cordinates;
				LocalPlayerInfo.Ped = Ped;
				LocalPlayerInfo.iIndex = i;
			}

			try
			{
				EntityList.push_back(CurrentEntity);
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
			}

		}

		LockLists.unlock();
	}

	bool FivemSDK::UpdateVehicles()
	{
		if (!pCamGameplayDirector)
			return false;

		if (!pVehicleInterface)
		{
			pVehicleInterface = pReplayInterface->VehicleInterface();

			if (!pVehicleInterface)
				return false;
		}

		LockLists2.lock();

		VehicleList.clear();
		VehicleList.shrink_to_fit();

		for (size_t i = 0; i < pVehicleInterface->VehicleMaximum(); i++)
		{
			CVehicle* Vehicle = pVehicleInterface->VehicleList()->Vehicle(i);
			if (!Vehicle)
				continue;

			if (Vehicle->GetCoordinate().DistTo(GetLocalPlayerInfo().WorldPos) > 600)
				continue;

			VehicleInfo CurrentVeh;

			CurrentVeh.Vehicle = Vehicle;
			CurrentVeh.ModelInfo = Vehicle->GetModelInfo();
			if (CurrentVeh.ModelInfo)
			{
				CurrentVeh.Name = FrameWork::Memory::ReadProcessMemoryString(CurrentVeh.ModelInfo + 0x298, 24);
				if (CurrentVeh.Name.find('?') != std::string::npos)
					continue;
			}

			CurrentVeh.iIndex = i;

			try
			{
				VehicleList.push_back(CurrentVeh);
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
			}
		}

		LockLists2.unlock();
	}

	void FivemSDK::TeleportToObject(uintptr_t Object, uintptr_t Navigation, uintptr_t ModelInfo, Vector3D Position, Vector3D VisualPosition, bool Stop)
	{
		float BackupMagic = 0.f;
		if (Stop)
		{
			BackupMagic = FrameWork::Memory::ReadMemory<float>(ModelInfo + 0x2C);
			FrameWork::Memory::WriteMemory<float>(ModelInfo + 0x2C, 0.f);
		}

		FrameWork::Memory::WriteMemory<Vector3D>(Object + 0x90, VisualPosition);
		FrameWork::Memory::WriteMemory<Vector3D>(Navigation + 0x50, Position);

		if (Stop)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(40));
			FrameWork::Memory::WriteMemory<float>(ModelInfo + 0x2C, BackupMagic);
		}
	}

	uint64_t FivemSDK::SyncTreeGetter(std::string SyncName)
	{
		uint64_t SyncTree = NetSync;
		uint64_t SyncTreeOffset;

		std::map<std::string, uint64_t> SyncTreeTypes =
		{
			{XorStr("CPedSyncTree"), SyncTree + 0x26},
			{XorStr("CObjectSyncTree"), SyncTree + 0x2E},
			{XorStr("CHeliSyncTree"), SyncTree + 0x36},
			{XorStr("CDoorSyncTree"), SyncTree + 0x3E},
			{XorStr("CBoatSyncTree"), SyncTree + 0x46},
			{XorStr("CBikeSyncTree"), SyncTree + 0x4E},
			{XorStr("CAutomobileSyncTree"), SyncTree + 0x56},
			{XorStr("CPickupSyncTree"), SyncTree + 0x5E},
			{XorStr("CTrainSyncTree"), SyncTree + 0x82},
			{XorStr("CPlayerSyncTree"), SyncTree + 0x8A},
			{XorStr("CSubmarineSyncTree"), SyncTree + 0x92},
			{XorStr("CPlaneSyncTree"), SyncTree + 0x9A},
			{XorStr("CPickupPlacementSyncTree"), SyncTree + 0xA2}
		};

		auto it = SyncTreeTypes.find(SyncName);
		if (it != SyncTreeTypes.end())
			SyncTreeOffset = it->second;

		return FrameWork::Memory::ReadMemory<uint64_t>(SyncTreeOffset + FrameWork::Memory::ReadMemory<int>(SyncTreeOffset + 3) + 7);
	}

	ImVec2 FivemSDK::WorldToScreen(Vector3D Pos)
	{
		if (!pViewPort)
		{
			pViewPort = FrameWork::Memory::ReadMemory<uint64_t>(ViewPort);
		}

		Matrix4x4 ViewMatrix = FrameWork::Memory::ReadMemory<Matrix4x4>(pViewPort + 0x24C);

		ViewMatrix.TransposeThisMatrix();

		Vector4D VecX(ViewMatrix._21, ViewMatrix._22, ViewMatrix._23, ViewMatrix._24);
		Vector4D VecY(ViewMatrix._31, ViewMatrix._32, ViewMatrix._33, ViewMatrix._34);
		Vector4D VecZ(ViewMatrix._41, ViewMatrix._42, ViewMatrix._43, ViewMatrix._44);

		Vector3D ScreenPos;
		ScreenPos.x = (VecX.x * Pos.x) + (VecX.y * Pos.y) + (VecX.z * Pos.z) + VecX.w;
		ScreenPos.y = (VecY.x * Pos.x) + (VecY.y * Pos.y) + (VecY.z * Pos.z) + VecY.w;
		ScreenPos.z = (VecZ.x * Pos.x) + (VecZ.y * Pos.y) + (VecZ.z * Pos.z) + VecZ.w;

		if (ScreenPos.z <= 0.1f)
			return ImVec2(0, 0);

		ScreenPos.z = 1.0f / ScreenPos.z;
		ScreenPos.x *= ScreenPos.z;
		ScreenPos.y *= ScreenPos.z;

		const ImVec2 projection = FrameWork::Overlay::GetProjectionSize();
		ScreenPos.x = projection.x / 2.f + (0.5f * ScreenPos.x * projection.x + 0.5f);
		ScreenPos.y = projection.y / 2.f - (0.5f * ScreenPos.y * projection.y + 0.5f);

		if (FrameWork::Overlay::IsSecondMonitorEspActive())
		{
			const ImVec2 scale = FrameWork::Overlay::GetEspScale();
			ScreenPos.x *= scale.x;
			ScreenPos.y *= scale.y;
		}

		return ImVec2(ScreenPos.x, ScreenPos.y);
	}

	CPed* FivemSDK::GetAimingEntity()
	{
		return (CPed*)FrameWork::Memory::ReadMemory<uint64_t>(PlayerAimingAt);
	}

	Vector3D GetBonePosByInstFragAndID(uint64_t crSkeletonData, unsigned int BoneID)
	{
		Matrix4x4 v4 = FrameWork::Memory::ReadMemory<Matrix4x4>(FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x8));
		Matrix4x4 Result = FrameWork::Memory::ReadMemory<Matrix4x4>(FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x18) + (BoneID << 6));

		Vector3D vec1(v4._11, v4._12, v4._13);
		Vector3D vec2(v4._21, v4._22, v4._23);
		Vector3D vec3(v4._31, v4._32, v4._33);
		Vector3D vec4(v4._41, v4._42, v4._43);
		Vector3D vec5(Result._41, Result._42, Result._43);

		return Vector3D(
			vec1.x * vec5.x + vec4.x + vec2.x * vec5.y + vec3.x * vec5.z,
			vec1.y * vec5.x + vec4.y + vec2.y * vec5.y + vec3.y * vec5.z,
			vec1.z * vec5.x + vec4.z + vec2.z * vec5.y + vec3.z * vec5.z
		);
	}

	ImVec2 FivemSDK::GetClosestHitBox(Entity Ped)
	{
		ImVec2 Result = ImVec2(0, 0);

		ImVec2 Head = Ped.HeadPos;
		if (!g_Fivem.IsOnScreen(Head))
			return Result;

		ImVec2 Neck = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Ped, SKEL_Neck_1));
		if (!g_Fivem.IsOnScreen(Neck))
			return Result;

		ImVec2 Chest = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Ped, SKEL_Spine3));
		if (!g_Fivem.IsOnScreen(Chest))
			return Result;

		ImVec2 Center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

		float HeadDistance = Head.DistTo(Center);
		float NeckDistance = Neck.DistTo(Center);
		float ChestDistance = Chest.DistTo(Center);

		float DistancesArray[] = { HeadDistance, NeckDistance, ChestDistance };

		float Closest = DistancesArray[0];
		int ClosestBone = 0;

		for (int i = 0; i < 5; ++i)
		{
			if (DistancesArray[i] < Closest)
			{
				Closest = DistancesArray[i];
				ClosestBone = i;
			}
		}

		switch (ClosestBone)
		{
		case 0:
			return Head;
			break;
		case 1:
			return Neck;
			break;
		case 2:
			return Chest;
			break;
		default:
			return Head;
			break;
		}

	}

	bool FivemSDK::FindClosestEntity(float Fov, int MaxDistance, bool VisibleCheck, bool NPC, int PriorityMode, Entity* Output)
	{
		Entity Closest;
		float ClosestWorldDistance = FLT_MAX;
		float ClosestScreenDistance = FLT_MAX;
		float BestHealth = FLT_MAX;

		bool Found = false;

		LockLists.lock();
		for (Entity Current : EntityList)
		{
			if (Current.StaticInfo.bIsLocalPlayer)
				continue;

			if (Current.StaticInfo.bIsNPC && !NPC)
				continue;

			if (!Current.StaticInfo.VisibleCheck && VisibleCheck)
				continue;

			float WorldDistance = Current.Cordinates.DistTo(GetLocalPlayerInfo().WorldPos);
			if (WorldDistance > MaxDistance)
				continue;

			ImVec2 ClosestBone = GetClosestHitBox(Current);
			if (ClosestBone.x == 0 && ClosestBone.y == 0)
				continue;

			ImVec2 Center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
			float ScreenDistance = ClosestBone.DistTo(Center);

			// Se Fov180 está ativado, ignora o check de FOV (pega todos)
			// Se Fov180 está desativado, aplica o check normal de FOV
			// Verifica se está dentro do FOV
			if (ScreenDistance > Fov)
				continue;


			float CurrentHealth = 99999.f;
			if (Current.StaticInfo.Ped)
				CurrentHealth = Current.StaticInfo.Ped->GetHealth();

			switch (PriorityMode)
			{
			case 0: // FOV
				if (ScreenDistance < ClosestScreenDistance)
				{
					ClosestScreenDistance = ScreenDistance;
					Closest = Current;
					Found = true;
				}
				break;

			case 1: // Distância
				if (WorldDistance < ClosestWorldDistance)
				{
					ClosestWorldDistance = WorldDistance;
					Closest = Current;
					Found = true;
				}
				break;

			case 2: // Health
				if (CurrentHealth > 0 && CurrentHealth < BestHealth)
				{
					BestHealth = CurrentHealth;
					Closest = Current;
					Found = true;
				}
				break;
			}
		}

		if (Found)
			*Output = Closest;

		LockLists.unlock();
		return Found;
	}

	void FivemSDK::ProcessCameraMovement(Vector3D WorldPosition, int SmoothHorizontal, int SmoothVertical)
	{
		if (!pCamGameplayDirector)
			return;

		auto FollowPedCamera = pCamGameplayDirector->GetFollowPedCamera();

		Vector3D CrosshairPosition = FollowPedCamera->GetCrosshairPosition();
		Vector3D ViewAngles = FollowPedCamera->GetViewAngles();

		float Distance = CrosshairPosition.DistTo(WorldPosition);
		
		// Avoid division by zero
		if (Distance <= 0.0001f)
			return;

		Vector3D AimAngles = Vector3D(
			(WorldPosition.x - CrosshairPosition.x) / Distance, 
			(WorldPosition.y - CrosshairPosition.y) / Distance, 
			(WorldPosition.z - CrosshairPosition.z) / Distance
		);

		Vector3D FinalAngles = AimAngles;

		// Calculate camera delta
		Vector3D CameraDelta = Vector3D(
			AimAngles.x - ViewAngles.x, 
			AimAngles.y - ViewAngles.y, 
			AimAngles.z - ViewAngles.z
		);

		// Apply smooth (clamp to minimum 1)
		const int clampedSmoothH = (SmoothHorizontal < 1) ? 1 : SmoothHorizontal;
		const int clampedSmoothV = (SmoothVertical < 1) ? 1 : SmoothVertical;
		
		if (clampedSmoothH > 1 || clampedSmoothV > 1)
		{
			FinalAngles.x = ViewAngles.x + CameraDelta.x / static_cast<float>(clampedSmoothH);
			FinalAngles.y = ViewAngles.y + CameraDelta.y / static_cast<float>(clampedSmoothH);
			FinalAngles.z = ViewAngles.z + CameraDelta.z / static_cast<float>(clampedSmoothV);
		}

		float AimbotFixZ = ViewAngles.z - FollowPedCamera->GetThirdpersonViewAngles().z;

		Vector3D ThirdPersonAngles = FinalAngles;
		ThirdPersonAngles.z = ThirdPersonAngles.z - AimbotFixZ;

		FollowPedCamera->SetThirdpersonViewAngles(ThirdPersonAngles);
		FollowPedCamera->SetViewAngles(FinalAngles);
	}

	Vector3D FivemSDK::GetBonePosVec3(Entity Ped, unsigned int Mask)
	{
		uint64_t FragInstNMGta = FrameWork::Memory::ReadMemory<uint64_t>(Ped.StaticInfo.Ped + Offsets::FragInsNmGTA);
		if (FragInstNMGta)
		{
			Ped.StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(FragInstNMGta + 0x68) + 0x178);

			auto it = Ped.StaticInfo.MaskToBoneId.find(Mask);
			if (it == Ped.StaticInfo.MaskToBoneId.end())
			{
				unsigned int BoneId = 0;
				if (GetPedBoneIndex(Ped, Mask, BoneId))
				{
					if (BoneId)
					{
						Ped.StaticInfo.MaskToBoneId[Mask] = BoneId;
						return GetBonePosByInstFragAndID(Ped.StaticInfo.crSkeletonData, BoneId);
					}
				}
			}
			else
			{
				return GetBonePosByInstFragAndID(Ped.StaticInfo.crSkeletonData, it->second);
			}
		}

		return Ped.Cordinates;
	}

	bool FivemSDK::GetPedBoneIndex(Entity Ped, unsigned int Mask, unsigned int& newIdx)
	{
		uint64_t crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(Ped.StaticInfo.crSkeletonData);

		if (FrameWork::Memory::ReadMemory<int16_t>(crSkeletonData + 0x1A))
		{
			uint16_t v1 = FrameWork::Memory::ReadMemory<uint16_t>(crSkeletonData + 0x18);
			if (v1)
			{
				int64_t v2 = FrameWork::Memory::ReadMemory<int64_t>(crSkeletonData + 0x10);
				int Count = 0;
				for (int64_t i = FrameWork::Memory::ReadMemory<int64_t>(v2 + 0x8 * (Mask % v1)); ; i = FrameWork::Memory::ReadMemory<int64_t>(i + 0x8))
				{
					Count++;
					if (!i || i >= 0xCCCCCCCCCCCCCC || Count > 3)
						return false;

					int v5 = FrameWork::Memory::ReadMemory<int>(i);
					if (Mask == v5)
					{
						int v6 = FrameWork::Memory::ReadMemory<int>(i + 0x4);
						newIdx = v6;
						return true;
					}
				}
			}
		}
		else if (Mask < FrameWork::Memory::ReadMemory<uint64_t>(crSkeletonData + 0x5E))
		{
			newIdx = Mask;
			return true;
		}

		return false;
	}

	uint32_t GetRelayIpAddress(uint64_t PeerAddress)
	{
		if (g_Fivem.GetGameVersion() >= 2824)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 72);

		if (g_Fivem.GetGameVersion() >= 2372)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 84);

		if (g_Fivem.GetGameVersion() >= 2060)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 20);

		return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 20);
	}

	uint32_t GetPublicIpAddress(uint64_t PeerAddress)
	{
		if (g_Fivem.GetGameVersion() >= 2824)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 168);

		if (g_Fivem.GetGameVersion() >= 2372)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 60);

		return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 28);
	}

	uint32_t GetLocalIpAddress(uint64_t PeerAddress)
	{
		if (g_Fivem.GetGameVersion() >= 2824)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 176);

		if (g_Fivem.GetGameVersion() >= 2372)
			return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 76);

		return FrameWork::Memory::ReadMemory<uint32_t>(PeerAddress + 36);
	}

	int ServerIdFromPeerAddress(uint64_t peerAddress)
	{
		if (!peerAddress || peerAddress == 0x20 || !ValidRemotePtr(peerAddress))
			return -1;

		const uint32_t ips[] = {
			GetLocalIpAddress(peerAddress),
			GetRelayIpAddress(peerAddress),
			GetPublicIpAddress(peerAddress),
			FrameWork::Memory::ReadMemory<uint32_t>(peerAddress + 36),
			FrameWork::Memory::ReadMemory<uint32_t>(peerAddress + 76),
			FrameWork::Memory::ReadMemory<uint32_t>(peerAddress + 176),
		};

		for (uint32_t ip : ips)
		{
			const int encoded = FeedNetIdFromIp(ip);
			if (encoded >= 0)
				return encoded;
		}

		return ScanEncodedServerId(peerAddress, {});
	}

	std::string FivemSDK::GetPlayerName(uint64_t PeerAddress, int GameNetId)
	{
		const int peerNetId = ServerIdFromPeerAddress(PeerAddress);
		const int lookupIds[] = { GameNetId, peerNetId };

		{
			std::lock_guard<std::mutex> namesLock(LockPlayerList);
			for (int id : lookupIds)
			{
				if (id < 0)
					continue;
				auto it = PlayerNamesMap.find(id);
				if (it != PlayerNamesMap.end() && LooksLikePlayerName(it->second))
					return it->second;
			}
		}

		if (PeerAddress == 0x20)
			return {};

		static std::unordered_map<int, std::string> g_NetToIdNames;
		for (int id : lookupIds)
		{
			if (id < 0)
				continue;
			auto cached = g_NetToIdNames.find(id);
			if (cached != g_NetToIdNames.end() && LooksLikePlayerName(cached->second))
				return cached->second;
		}

		std::string Result;

		if (g_Fivem.GetGameVersion() < 2824)
		{
			uint32_t relayIp = GetRelayIpAddress(PeerAddress);
			uint32_t publicIp = GetPublicIpAddress(PeerAddress);
			uint32_t localIp = GetLocalIpAddress(PeerAddress);
			if (relayIp != localIp || relayIp != publicIp)
			{
				const int nameOff = (g_Fivem.GetGameVersion() < 2060) ? 84
					: (g_Fivem.GetGameVersion() < 2372) ? 92 : 132;
				Result = FrameWork::Memory::ReadProcessMemoryString(PeerAddress + nameOff, 64);
				if (LooksLikePlayerName(Result))
					return Result;
			}
		}

		int NetId = peerNetId >= 0 ? peerNetId : GameNetId;
		if (NetId < 0)
			return Result;

		const uintptr_t mapAddr = ResolveNameMapObject(NetIdToNamesPtr);
		const uintptr_t tableMap = mapAddr ? mapAddr : NetIdToNamesPtr;
		if (!tableMap)
			return Result;

		uint64_t HashPart1 = static_cast<uint8_t>(NetId) ^ 0xCBF29CE484222325;
		uint64_t HashPart2 = static_cast<uint8_t>(NetId >> 8) ^ 0x100000001B3 * HashPart1;
		uint64_t HashPart3 = static_cast<uint8_t>(NetId >> 16) ^ 0x100000001B3 * HashPart2;
		uint64_t HashPart4 = static_cast<uint8_t>(NetId >> 24) ^ 0x100000001B3 * HashPart3;
		uint64_t Hash = 0x100000001B3 * HashPart4;

		uint64_t HashMask = FrameWork::Memory::ReadMemory<uint64_t>(tableMap + 0x28);
		uint64_t HashPosition = Hash & HashMask;

		uint64_t TableBase = FrameWork::Memory::ReadMemory<uint64_t>(tableMap + 0x10);
		if (!ValidRemotePtr(TableBase))
			return Result;

		uint64_t IndexAddress = TableBase + 0x10 * HashPosition;
		uint64_t Index = FrameWork::Memory::ReadMemory<uint64_t>(IndexAddress + sizeof(uint64_t));
		if (!Index)
			return Result;

		uint64_t EndAddress = FrameWork::Memory::ReadMemory<uint64_t>(tableMap);
		if (Index == EndAddress)
			return Result;

		uint64_t InitialValue = FrameWork::Memory::ReadMemory<uint64_t>(Index);

		if (NetId != static_cast<int>(FrameWork::Memory::ReadMemory<DWORD>(Index + 0x10)))
		{
			int guard = 0;
			while (Index != InitialValue && guard++ < 32)
			{
				Index = FrameWork::Memory::ReadMemory<uint64_t>(Index + 8);
				if (!ValidRemotePtr(Index))
					break;
				if (NetId == static_cast<int>(FrameWork::Memory::ReadMemory<DWORD>(Index + 0x10)))
					break;
			}
		}

		if (Index && Index != EndAddress)
		{
			Result = ReadNameAtNode(Index);
			if (LooksLikePlayerName(Result))
			{
				g_NetToIdNames[NetId] = Result;
				if (GameNetId >= 0)
					g_NetToIdNames[GameNetId] = Result;
			}
		}

		return Result;
	}

	void FivemSDK::UpdatePlayerNamesThread()
	{
		while (!g_Options.General.ShutDown)
		{
			if (!pLocalPlayer)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				continue;
			}

			if (!CitizemPlayerNamesModule)
			{
				CitizemPlayerNamesModule = FrameWork::Memory::GetModuleBaseByName(Pid, XorStr(L"citizen-playernames-five.dll"));
				if (!CitizemPlayerNamesModule)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(500));
					continue;
				}
			}

			if (!ResolveNameMapObject(NetIdToNamesPtr))
			{
				const uint64_t playerNamesSize = FrameWork::Memory::GetModuleSizeByName(Pid, XorStr(L"citizen-playernames-five.dll"));
				NetIdToNamesPtr = LocatePlayerNamesMap(CitizemPlayerNamesModule, playerNamesSize);
			}

			if (NetIdToNamesPtr)
			{
				auto temp_names = HarvestPlayerNameMap(NetIdToNamesPtr);
				if (!temp_names.empty())
				{
					std::lock_guard<std::mutex> namesLock(LockPlayerList);
					PlayerNamesMap = std::move(temp_names);
				}
			}

			std::this_thread::sleep_for(std::chrono::milliseconds(1500));
		}
	}

	//std::string FivemSDK::GetPlayerName(uint64_t PeerAddress, int GameNetId)
	//{
	//	static std::unordered_map<int, std::string> g_NetToIdNames;

	//	auto it = g_NetToIdNames.find(GameNetId);

	//	if (it != g_NetToIdNames.end())
	//	{
	//		return it->second.c_str();
	//	}

	//	std::string Result = XorStr("** Invalid **");

	//	if (PeerAddress == 0x20)
	//		return Result;

	//	uint32_t RelayIpAddr = GetRelayIpAddress(PeerAddress);
	//	uint32_t PublicIpAddr = GetPublicIpAddress(PeerAddress);
	//	uint32_t LocalAddr = GetLocalIpAddress(PeerAddress);

	//	int NetId = 0;
	//	uint32_t CurAddress = 0;

	//	if (g_Fivem.GetGameVersion() < 2944)
	//	{
	//		if (g_Fivem.GetGameVersion() < 2372)
	//		{
	//			if (g_Fivem.GetGameVersion() < 2060)
	//			{
	//				if (RelayIpAddr != LocalAddr || RelayIpAddr != PublicIpAddr)
	//					return FrameWork::Memory::ReadProcessMemoryString(PeerAddress + 84, 64).c_str();
	//			}
	//			else
	//			{
	//				if (RelayIpAddr != LocalAddr || RelayIpAddr != PublicIpAddr)
	//					return FrameWork::Memory::ReadProcessMemoryString(PeerAddress + 92, 64).c_str();
	//			}
	//		}
	//		else
	//		{
	//			if (RelayIpAddr != LocalAddr || RelayIpAddr != PublicIpAddr)
	//				return FrameWork::Memory::ReadProcessMemoryString(PeerAddress + 132, 64).c_str();
	//		}

	//		CurAddress = RelayIpAddr;
	//	}
	//	else
	//	{
	//		CurAddress = LocalAddr;
	//	}

	//	NetId = (CurAddress & 0xFFFF) ^ 0xFEED;

	//	uint64_t HashPart1 = static_cast<uint8_t>(NetId) ^ 0xCBF29CE484222325;
	//	uint64_t HashPart2 = static_cast<uint8_t>(NetId >> 8) ^ 0x100000001B3 * HashPart1;
	//	uint64_t HashPart3 = static_cast<uint8_t>(NetId >> 16) ^ 0x100000001B3 * HashPart2;
	//	uint64_t HashPart4 = static_cast<uint8_t>(NetId >> 24) ^ 0x100000001B3 * HashPart3;
	//	uint64_t Hash = 0x100000001B3 * HashPart4;

	//	uint64_t HashMask = FrameWork::Memory::ReadMemory<uint64_t>(g_Fivem.NetIdToNamesPtr + 0x28);
	//	uint64_t HashPosition = Hash & HashMask;

	//	uint64_t TableBase = FrameWork::Memory::ReadMemory<uint64_t>(g_Fivem.NetIdToNamesPtr + 0x10);
	//	uint64_t IndexAddress = TableBase + 0x10 * HashPosition;

	//	uint64_t Index = FrameWork::Memory::ReadMemory<uint64_t>(IndexAddress + sizeof(uint64_t));
	//	if (!Index)
	//		return Result;

	//	uint64_t EndAddress = FrameWork::Memory::ReadMemory<uint64_t>(g_Fivem.NetIdToNamesPtr);
	//	if (Index == EndAddress)
	//		return Result;

	//	uint64_t InitialValue = FrameWork::Memory::ReadMemory<uint64_t>(Index);

	//	if (NetId != FrameWork::Memory::ReadMemory<DWORD>(Index + 0x10))
	//	{
	//		while (Index != InitialValue)
	//		{
	//			Index = FrameWork::Memory::ReadMemory<uint64_t>(Index + 8);
	//			if (NetId == FrameWork::Memory::ReadMemory<DWORD>(Index + 0x10))
	//				break;
	//		}
	//	}

	//	if (Index != EndAddress)
	//	{
	//		Result = FrameWork::Memory::ReadProcessMemoryString(Index + 0x18, 128);
	//		g_NetToIdNames[GameNetId] = Result;
	//	}

	//	return Result;
	//}

	bool FivemSDK::IsOnScreen(ImVec2 Pos)
	{
		if (Pos.x < 0.1f || Pos.y < 0.1 || Pos.x > ImGui::GetIO().DisplaySize.x || Pos.y > ImGui::GetIO().DisplaySize.y)
			return false;

		return true;
	}
}