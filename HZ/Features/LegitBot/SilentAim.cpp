#include "SilentAim.hpp"
#include "../../FiveM-External.hpp"
#include "../../Definations/Variables.hpp"
#include "../../FivemSDK/Offsets.hpp"
#include "../../Render/Overlay.hpp"
#include "../../Utils/DebugLog.hpp"
#include <cmath>

namespace Cheat
{
	namespace
	{
		constexpr size_t kSilentAimPatchSize = 38;
		constexpr size_t kSilentAimReturnOffset = 0x26;

		bool SilentAimInitialized = false;
		uintptr_t StartAddy = 0;
		uintptr_t SilentAimHook = 0;
		std::vector<uint8_t> OriginalFuncTable;
		std::vector<uint8_t> SilentAimShell =
		{
			0xC7, 0x45, 0x07,
			0x00, 0x00, 0x00, 0x00,

			0xC7, 0x45, 0x0B,
			0x00, 0x00, 0x00, 0x00,

			0xC7, 0x45, 0x0F,
			0x00, 0x00, 0x00, 0x00,

			0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};

		uintptr_t ResolveSilentAimAddress()
		{
			if (g_Fivem.HandleBulletMagic)
				return g_Fivem.HandleBulletMagic;

			if (!g_Fivem.GetModuleBase() || !g_Fivem.ModuleBaseSize)
				return 0;

			return FrameWork::Memory::FindSignature(
				{ 0x48, 0x8D, 0x45, 0x00, 0xF3, 0x0F, 0x10, 0x00, 0xF3, 0x0F, 0x10, 0x48, 0x00, 0xF3, 0x0F, 0x11, 0x45 },
				g_Fivem.GetModuleBase(),
				g_Fivem.ModuleBaseSize);
		}

		bool ShouldActivateSilent()
		{
			if (g_Options.LegitBot.SilentAim.KeyBind == 0)
				return true;

			return (SafeCall(GetAsyncKeyState)(g_Options.LegitBot.SilentAim.KeyBind) & 0x8000) != 0;
		}

		bool IsTargetingAllowed()
		{
			HWND overlayWindow = FrameWork::Overlay::GetOverlayWindow();
			HWND foregroundWindow = SafeCall(GetForegroundWindow)();
			return !overlayWindow || foregroundWindow != overlayWindow;
		}

		void RestoreSilent()
		{
			if (!SilentAimInitialized || !StartAddy)
			{
				SilentAimInitialized = false;
				return;
			}

			if (!OriginalFuncTable.empty())
				FrameWork::Memory::WriteBytes(StartAddy, OriginalFuncTable);

			if (SilentAimHook)
				FrameWork::Memory::FreeCave(SilentAimHook);

			SilentAimHook = 0;
			StartAddy = 0;
			OriginalFuncTable.clear();
			SilentAimInitialized = false;
		}

		bool InitializeSilentAim()
		{
			StartAddy = ResolveSilentAimAddress();
			if (!StartAddy)
			{
				static bool logged = false;
				if (!logged)
				{
					MELLO_DBG("[SilentAim] Silent aim signature not found.");
					logged = true;
				}
				return false;
			}

			SilentAimHook = FrameWork::Memory::CreateCodeCave(500);
			if (!SilentAimHook)
				return false;

			OriginalFuncTable = FrameWork::Memory::ReadBytes(StartAddy, kSilentAimPatchSize);
			if (OriginalFuncTable.size() != kSilentAimPatchSize)
				return false;

			if (!FrameWork::Memory::WriteBytes(SilentAimHook, SilentAimShell))
				return false;

			if (!FrameWork::Memory::HookJump(StartAddy, SilentAimHook))
				return false;

			return true;
		}

		void ApplySilentAt(Vector3D targetPos)
		{
			if (!SilentAimInitialized || !SilentAimHook || !StartAddy)
				return;

			const bool miss = g_Options.LegitBot.SilentAim.MissChance > 0
				&& (rand() % 100) < g_Options.LegitBot.SilentAim.MissChance;

			Vector3D finalPos = miss
				? Vector3D{ targetPos.x, targetPos.y + 0.4f, targetPos.z }
				: Vector3D{ targetPos.x, targetPos.y, targetPos.z + 0.08f };

			memcpy(SilentAimShell.data() + 3, &finalPos.x, sizeof(float));
			memcpy(SilentAimShell.data() + 10, &finalPos.y, sizeof(float));
			memcpy(SilentAimShell.data() + 17, &finalPos.z, sizeof(float));

			const uintptr_t backAddress = StartAddy + kSilentAimReturnOffset;
			memcpy(SilentAimShell.data() + 27, &backAddress, sizeof(backAddress));

			FrameWork::Memory::WriteBytes(SilentAimHook, SilentAimShell);
		}

		uintptr_t GetCWeaponObject()
		{
			auto localPlayer = g_Fivem.GetLocalPlayerInfo().Ped;
			if (!localPlayer)
				return 0;

			const uintptr_t weaponManager = reinterpret_cast<uintptr_t>(localPlayer->GetWeaponManager());
			if (!weaponManager)
				return 0;

			const uintptr_t weaponObj = FrameWork::Memory::ReadMemory<uintptr_t>(weaponManager + Offsets::CObject);
			if (!weaponObj)
				return 0;

			return FrameWork::Memory::ReadMemory<uintptr_t>(weaponObj + Offsets::CWeapon);
		}
	}

	void SilentAim::RestorePatch()
	{
		RestoreSilent();
	}

	void SilentAim::RunThread() noexcept
	{
		while (!g_Options.General.ShutDown.load(std::memory_order_relaxed))
		{
			if (!g_Options.LegitBot.SilentAim.Enabled
				|| !g_Fivem.IsInitialized()
				|| !ShouldActivateSilent()
				|| !IsTargetingAllowed())
			{
				if (SilentAimInitialized)
					RestoreSilent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			Entity closestEntity{};
			if (!g_Fivem.FindClosestEntity(
				g_Options.LegitBot.SilentAim.Fov,
				g_Options.LegitBot.SilentAim.MaxDistance,
				g_Options.LegitBot.SilentAim.VisibleCheck,
				g_Options.LegitBot.SilentAim.ShotNPC,
				g_Options.LegitBot.SilentAim.ClosestFov,
				&closestEntity)
				|| !closestEntity.StaticInfo.Ped
				|| closestEntity.StaticInfo.IsFriend)
			{
				if (SilentAimInitialized)
					RestoreSilent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			Vector3D bonePos{};
			switch (g_Options.LegitBot.SilentAim.HitBox)
			{
			case 1:
				bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Neck_1);
				break;
			case 2:
				bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Spine3);
				break;
			default:
				bonePos = g_Fivem.GetBonePosVec3(closestEntity, SKEL_Head);
				break;
			}

			if (bonePos.IsZero())
			{
				if (SilentAimInitialized)
					RestoreSilent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			ImVec2 screenPos = g_Fivem.WorldToScreen(bonePos);
			if (!g_Fivem.IsOnScreen(screenPos))
			{
				if (SilentAimInitialized)
					RestoreSilent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			const ImVec2 center(
				ImGui::GetIO().DisplaySize.x * 0.5f,
				ImGui::GetIO().DisplaySize.y * 0.5f);
			const float fovDistance = std::hypot(screenPos.x - center.x, screenPos.y - center.y);
			if (fovDistance >= g_Options.LegitBot.SilentAim.Fov)
			{
				if (SilentAimInitialized)
					RestoreSilent();
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
				continue;
			}

			if (!SilentAimInitialized)
			{
				if (!InitializeSilentAim())
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					continue;
				}
				SilentAimInitialized = true;
			}

			ApplySilentAt(bonePos);

			if (g_Options.LegitBot.SilentAim.MagicBullet)
			{
				if (uintptr_t cWeapon = GetCWeaponObject())
					FrameWork::Memory::WriteMemory<Vector3D>(cWeapon + 0x20, bonePos);
			}

			std::this_thread::sleep_for(std::chrono::nanoseconds(10));
		}

		RestoreSilent();
	}
}
