#pragma once
#include <cstdint>
#include <mutex>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>

#include "Classes.hpp"
#include "../ImGui/imgui.h"

namespace Cheat
{
	struct LocalPEDInfo
	{
		CPed* Ped;

		Vector3D WorldPos;

		int iIndex;
	};

	struct VehicleInfo
	{
		CVehicle* Vehicle;
		std::string Name;
		uint64_t ModelInfo;
		int iIndex = -1;
		bool Visible = false;
	};

	struct NetworkInfo
	{
		std::string UserName;
		std::string SteamId;
		std::string DiscordId;
	};

	struct PedStaticInfo
	{
		CPed* Ped;

		uint64_t crSkeletonData;

		std::unordered_map<unsigned int, unsigned> MaskToBoneId;
		std::string Name;

		int NetId;
		int iIndex;

		bool IsFriend = false;
		bool bIsLocalPlayer;
		bool bIsNPC;
		bool VisibleCheck;
	};

	struct Entity
	{
		PedStaticInfo StaticInfo;
		NetworkInfo NetworkInfo;

		Vector3D Cordinates;

		bool Visible;
		ImVec2 HeadPos;
	};

	class FivemSDK
	{
	public:
		void Intialize();
		bool UpdateEntities();
		bool UpdateVehicles();

		DWORD GetPid() { return Pid; }
		uint64_t GetModuleBase() { return ModuleBase; };

		LocalPEDInfo GetLocalPlayerInfo() { return LocalPlayerInfo; }
		CCamGameplayDirector* GetCamGameplayDirector() { return pCamGameplayDirector; }
		std::vector<Entity> GetEntitiyList() { return EntityList; }
		std::vector<VehicleInfo> GetVehicleList() { return VehicleList; }
		uint64_t GetBlipListAddress() { return BlipList; }

		uint64_t GetInvisibleAddress() { return Invisible; }
		uint64_t GetDsyncAddress() { return DSync; }
		uint64_t GetCanCombatRollAddress() { return CanCombatRoll; }

		CPed* GetAimingEntity();

		Vector3D GetBonePosVec3(Entity Ped, unsigned int Mask);
		bool GetPedBoneIndex(Entity Ped, unsigned int Mask, unsigned int& newIdx);

		void TeleportToObject(uintptr_t Object, uintptr_t Navigation, uintptr_t ModelInfo, Vector3D Position, Vector3D VisualPosition, bool Stop);

		uint64_t SyncTreeGetter(std::string SyncTree);
		std::string GetPlayerName(uint64_t PeerAddress, int GameNetId);
		void UpdatePlayerNamesThread();

		ImVec2 WorldToScreen(Vector3D Pos);
		bool IsOnScreen(ImVec2 Pos);

		ImVec2 GetClosestHitBox(Entity Ped);
		bool FindClosestEntity(float Fov, int MaxDistance, bool VisibleCheck, bool NPC, int PriorityMode, Entity* Output);

		void ProcessCameraMovement(Vector3D WorldPosition, int SmoothHorizontal, int SmoothVertical);

		int GetGameVersion() { return RealGameVersion;}
		bool IsInitialized() { return bIsIntialized; }

		std::unordered_map<CPed*, PedStaticInfo> AllEntitesList;
		std::unordered_map<int, PedStaticInfo> FriendList;
		std::mutex LockLists;
		std::mutex LockLists2;

	public:
		bool bIsIntialized = false;

		uint64_t World;                    // 48 8B 05 ? ? ? ? 33 D2 48 8B 40 08 8A CA 48 85 C0 74 16 48 8B
		uint64_t ReplayInterface;          // 48 8B 05 ?? ?? ?? ?? 66 89 0D ?? ?? ?? ?? 4C 89 2C D0
		uint64_t Camera;			       // 48 8B 05 ? ? ? ? 38 98 ? ? ? ? 8A C3
		uint64_t ViewPort;                 // 48 8B 15 ? ? ? ? 48 8D 2D ? ? ? ? 48 8B CD

		uint64_t Invisible;                // 48 8B E8 33 C0 48 85 ED 0F 84
		uint64_t CanCombatRoll;            // 48 89 5C 24 ? 57 48 83 EC ? 48 8B D9 48 8B 89 ? ? ? ? BF ? ? ? ? 8B 81
		uint64_t NetSync;                  // 0F B7 CA 83 F9 07 7F 5E
		uint64_t DSync;					   // 75 2E 8B 4B 10 42 8D 04 09
		uint64_t NetIdToNamesPtr;		   // 48 8B CA 48 0F 45 C8
		uint64_t GameplayCamHolder;        // 4C 89 2D ? ? ? ? E8 ? ? ? ? E8
		uint64_t GameplayCamTarget;        // 48 83 EC 38 0F 29 74 24 ? 0F 28 F0 0F 2F 35 ? ? ? ? 73
		uint64_t BlipList;                 // 4C 8D 35 ? ? ? ? 3B 35 ? ? ? ? 74 ? 49 8B 3E

		uint64_t HandleBullet;             // F3 41 0F 10 19 F3 41 0F 10 41 04
		uint64_t PlayerAimingAt;           // 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 48 85 C9 74 05 E8 ?? ?? ?? ?? 8A CB

		DWORD Pid;
		uint64_t ModuleBase, ModuleBaseSize;
		uint64_t CitizemPlayerNamesModule;
		uintptr_t HandleBulletMagic = 0;
		std::string ModuleName;

		int GameVersion;
		int RealGameVersion;
	public:
		CWorld* pWorld;
		CPed* pLocalPlayer;
		CReplayInterface* pReplayInterface;
		CPedInterface* pPedInterface;
		CCamGameplayDirector* pCamGameplayDirector;
		CVehicleInterface* pVehicleInterface;

		uint64_t pViewPort;
	public:
		std::mutex LockPlayerList;
		std::map<int, std::string> PlayerNamesMap;
	public:
		std::vector<Entity> EntityList;
		std::vector<VehicleInfo> VehicleList;
		LocalPEDInfo LocalPlayerInfo;
	};
	inline FivemSDK g_Fivem;
}