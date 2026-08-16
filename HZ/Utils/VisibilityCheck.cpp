#include "VisibilityCheck.hpp"

namespace FrameWork
{
	namespace Visibility
	{
		static uint64_t GetOcclusionTableOffset()
		{
			switch (Cheat::g_Fivem.GetGameVersion())
			{
			case 1604: return 0x2559A50;
			case 2060: return 0x2559A50;
			case 2189: return 0x2559A50;
			case 2372: return 0x2559A50;
			case 2545: return 0x2559A50;
			case 2612: return 0x2559A50;
			case 2699: return 0x2559A50;
			case 2802: return 0x2559A50;
			case 2944: return 0x2559A50;
			case 3095: return 0x253C750;
			case 3258: return 0x2559A50;
			case 3323: return 0x2569B20;
			case 3407: return 0x257F060;
			case 3570: return 0x257F060;
			default:   return 0x2559A50;
			}
		}

		int checkpixels(Cheat::Entity& Current)
		{
			uintptr_t pedAddr = (uintptr_t)Current.StaticInfo.Ped;
			if (!pedAddr) return 0;

			uintptr_t moduleBase = Cheat::g_Fivem.GetModuleBase();
			if (!moduleBase) return 0;
			uint32_t originalFlags = FrameWork::Memory::ReadMemory<uint32_t>(pedAddr + 0xC0);
			if (!(originalFlags & (1 << 29)))
				FrameWork::Memory::WriteMemory<uint32_t>(pedAddr + 0xC0, originalFlags | (1 << 29));

			uintptr_t pedDrawHandler = FrameWork::Memory::ReadMemory<uintptr_t>(pedAddr + 0x48);
			if (!pedDrawHandler) return 0;

			uint32_t queryId = static_cast<uint32_t>(FrameWork::Memory::ReadMemory<uint8_t>(pedDrawHandler + 0x2B)) & 0xFF;
			if (queryId == 0 || queryId > 68)
				return 0;

			uintptr_t entryBase = moduleBase + GetOcclusionTableOffset() + ((queryId - 1) * 0x80);

			uint32_t flags   = FrameWork::Memory::ReadMemory<uint32_t>(entryBase + 0x7C);
			bool queryActive = (flags & 1) != 0;
			if (!queryActive) return 0;

			int pixelCount = FrameWork::Memory::ReadMemory<int>(entryBase + 0x78);
			if (pixelCount < 0) return 0;

			return pixelCount;
		}

		bool IsPlayerVisible(Cheat::Entity& Current, int threshold)
		{
			return checkpixels(Current) > threshold;
		}

		bool IsVehicleVisible(Cheat::CVehicle* Vehicle, int)
		{
			if (!Vehicle) return false;

			BYTE flag = FrameWork::Memory::ReadMemory<BYTE>((uintptr_t)Vehicle + Cheat::Offsets::VisibleFlag);

			if (flag == 36 || flag == 0 || flag == 4)
				return false;

			return true;
		}
	}
}
