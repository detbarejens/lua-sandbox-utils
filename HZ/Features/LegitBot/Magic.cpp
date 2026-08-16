#include "Magic.hpp"
#include "../../FiveM-External.hpp"
namespace Cheat
{
	namespace MagicBullets
	{
		static bool Initialized = false;

		static uintptr_t GetCWeaponObj()
		{
			auto localPlayer = g_Fivem.GetLocalPlayerInfo().Ped;
			if (!localPlayer) return 0;

			uintptr_t weaponManager = (uintptr_t)localPlayer->GetWeaponManager();
			if (!weaponManager) return 0;

			uintptr_t weaponObj = FrameWork::Memory::ReadMemory<uintptr_t>(weaponManager + Offsets::CObject);
			if (!weaponObj) return 0;

			return FrameWork::Memory::ReadMemory<uintptr_t>(weaponObj + Offsets::CWeapon);
		}

		static void Initialize()
		{
			if (!Offsets::MagicBulletsPatch)
				return;

			const uint8_t expected[] = { 0x0F, 0x29, 0x4F, 0x20 };
			for (size_t i = 0; i < sizeof(expected); ++i)
			{
				if (FrameWork::Memory::ReadMemory<uint8_t>(Offsets::MagicBulletsPatch + i) != expected[i])
					return;
			}

			std::vector<uint8_t> Nops = { 0x90, 0x90, 0x90, 0x90 };
			FrameWork::Memory::WriteBytes(Offsets::MagicBulletsPatch, Nops);
		}

		static void Restore()
		{
			if (!Offsets::MagicBulletsPatch) return;
			std::vector<uint8_t> RestoreBytes = { 0x0F, 0x29, 0x4F, 0x20 };
			FrameWork::Memory::WriteBytes(Offsets::MagicBulletsPatch, RestoreBytes);
		}

		void RestorePatch()
		{
			if (Initialized)
			{
				Restore();
				Initialized = false;
			}
		}

		void Start()
		{
			while (!g_Options.General.ShutDown)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(1));

				bool keyHeld = (g_Options.LegitBot.MagicBullets.KeyBind != 0)
					&& (SafeCall(GetAsyncKeyState)(g_Options.LegitBot.MagicBullets.KeyBind) & 0x8000);

				if (!g_Options.LegitBot.MagicBullets.Enabled || !keyHeld)
				{
					if (Initialized)
					{
						Restore();
						Initialized = false;
					}
					continue;
				}

				if (!g_Fivem.IsInitialized() || !g_Fivem.GetLocalPlayerInfo().Ped)
					continue;

				Entity ClosestEntity;
				bool found = g_Fivem.FindClosestEntity(
					g_Options.LegitBot.MagicBullets.FOV,
					g_Options.LegitBot.MagicBullets.MaxDistance,
					g_Options.LegitBot.MagicBullets.VisibleCheck,
					g_Options.LegitBot.MagicBullets.TargetNPC,
					false,
					&ClosestEntity
				);

				if (!found || !ClosestEntity.StaticInfo.Ped)
				{
					if (Initialized)
					{
						Restore();
						Initialized = false;
					}
					continue;
				}

				if (ClosestEntity.StaticInfo.IsFriend)
					continue;

				uintptr_t CWeapon = GetCWeaponObj();
				if (!CWeapon)
				{
					if (Initialized)
					{
						Restore();
						Initialized = false;
					}
					continue;
				}

				unsigned int BoneID = SKEL_Head;
				switch (g_Options.LegitBot.MagicBullets.HitBox)
				{
				case 1: BoneID = SKEL_Neck_1; break;
				case 2: BoneID = SKEL_Spine3;  break;
				default: BoneID = SKEL_Head;   break;
				}

				Vector3D TargetPos = g_Fivem.GetBonePosVec3(ClosestEntity, BoneID);

				ImVec2 ScreenPos = g_Fivem.WorldToScreen(TargetPos);
				if (!g_Fivem.IsOnScreen(ScreenPos))
					continue;

				if (!Initialized)
				{
					Initialize();
					Initialized = true;
				}

				Vector3D FinalPos = TargetPos;
				FinalPos.z += 0.30f;
				FrameWork::Memory::WriteMemory<Vector3D>(CWeapon + 0x20, FinalPos);
			}
		}
	}
}
