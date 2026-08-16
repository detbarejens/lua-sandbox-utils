#pragma once

#include <Windows.h>
#include <atomic>
#include <string>

enum class NoClipState
{
	Disabled,
	Hold,
	Toggle
};

namespace Cheat
{
	class Options
	{
	public:
		struct LegitBot
		{
			struct AimBot
			{
				bool Enabled;
				bool ShowFov;
				bool TargetNPC;
				bool VisibleCheck;
				bool StrictLock = false; // Lock forte que resiste ao movimento do mouse
				bool Prediction = false;
				bool AimAssist = false;
				bool StickyAim = false;
				bool DynamicFOV = false;

				int ClosestFov = 0;
				int KeyBind;
				int KeyBindState;
				int HitBox = 0;
				int MaxDistance = 250;
				int FOV = 100;
				int SmoothHorizontal = 0;
				int SmoothVertical = 0;
				int TargetPriority = 0; // 0=Distance, 1=Health, 2=Crosshair
				int AimAssistStrength = 50;
				int StickyAimStrength = 30;
				int PredictionMultiplier = 100;

				float FovColor[4] = { 0.5f, 0.5f, 0.5f, 1.f };
				float DynamicFOVMin = 50.f;
				float DynamicFOVMax = 150.f;
			}AimBot;

			struct SilentAim
			{
				bool Enabled = false;
				bool ShowFOV;
				bool ShotNPC;
				bool VisibleCheck;
				bool MagicBullet;
				bool Humanize;

				int Fov = 100;
				int KeyBind = 0;
				int KeyBindState;
				int MissChance = 0;
				int ClosestFov = 0;
				int MaxDistance = 250;
				int HitBox = 0;

				float FovColor[4] = { 0.5f, 0.5f, 0.5f, 1.f };
			}SilentAim;

			struct TriggerBot
			{
				bool Enabled;
				bool ShotNPC;
				bool VisibleCheck;
				bool ShowFOV;

				int KeyBind = 0;
				int KeyBindState = 0;
				int MaxDistance = 250;
				int ReactionTime;
				int FOV = 80;
				float FovColor[4] = { 0.2f, 1.f, 0.4f, 1.f };
			}Trigger;

			struct RapidFire
			{
				bool Enabled = false;
		
			}RapidFire;
			struct MagicBullets
			{
				bool Enabled;
				bool VisibleCheck;
				bool NoSpread;
				int KeyBind;
				int KeyBindState;
				int MaxDistance = 500;
				int FOV = 100;
				bool TargetNPC;
				int HitBox = 0;
				bool ShowFOV;
				bool WallBang;
				float FovColor[4] = { 1.f, 0.f, 1.f, 1.f };
			}MagicBullets;
		}LegitBot;

		struct Visuals
		{
			struct Players
			{
				bool Enabled;
				bool Toogle;
				bool ShowLocalPlayer;
				bool ShowNPC;
				bool VisibleCheck;
				bool ExcludeDeads;
				bool Chams = false;
				bool DamageNumbers = false;

				int ToggleKey = 0;
				int ToggleKeyState = 0;
				int RenderDistance = 250;
				int EspOptions = 6;
				int BoxType = 0;
				int HealthBarType = 1;
				int AmorBarType = 1;
				int VisiblePixelThreshold = 10; // Threshold de pixels para considerar visivel (> 150)
				int ChamsStyle = 0; // 0=Solid, 1=Wireframe, 2=Glow

				bool EnableBox;
				bool EnableDistance;
				bool EnableHeadBol;
				bool Skeleton;
				bool HealthBar;
				bool AmorBar;
				bool WeaponName;
				bool Name;
				bool RadarEnabled;
				bool LineToHead = false;
				// Novas opcoes
				bool ShowFriendTag = false;      // Tag [FRIEND] acima de amigos
				bool ShowPlayerFlags = false;    // Flags: armado, agachado, correndo
				bool ShowBoneHitbox = false;     // Circulo nos ossos principais (head/chest)
				bool ShowHealthGradient = false;
				bool ShowLookDirection = false;
				// Mais novas opcoes
				bool SnapLine = false;
				bool FilledBox = false;
				bool HealthText = false;
				bool OffscreenESP = false;
				bool CrosshairOnTarget = false;
				bool ShowNetID = false;
				bool ArmorText = false;
				bool CornerBox3D = false;
				// Criativas
				bool PulseCircle = false;
				bool DamageIndicator = false;
				bool ShadowBox = false;
				bool DistanceFade = false;
				bool TargetHighlight = false;
				// Mais criativas
				bool RainbowSkeleton = false;
				bool HealthRing = false;
				bool TrailEffect = false;
				bool ThreatLevel = false;
				bool CornerArrows = false;
				bool GlowOutline = false;
				// Novas opcoes criativas
				bool SpinningRing = false;
				bool BoxBreath = false;
				bool HealthBar3D = false;
				bool EnemyArrow = false;
				bool DistanceRings = false;
				bool ScanLine = false;
				bool ObserverAlert = false;
				bool ObserverAlertShowHud = true;
				float ObserverAlertHudX = 0.5f;
				float ObserverAlertHudY = 0.56f;
				int ObserverMaxDistance = 250;
				int ObserverLookThreshold = 92;
				int ObserverCanSeeFov = 55;
				bool ObserverVisibleCheck = false;
				bool AimingAlert = false;
				bool AimingAlertShowHud = true;
				float AimingAlertHudX = 0.5f;
				float AimingAlertHudY = 0.64f;
				int AimingMaxDistance = 200;
				int AimingLookThreshold = 90;
				int AimingAwarenessFov = 35;
				bool AimingVisibleCheck = false;

				float SnapLineColor[4] = { 1.f, 1.f, 0.f, 0.8f };
				float FilledBoxColor[4] = { 1.f, 1.f, 1.f, 0.06f };
				float OffscreenColor[4] = { 1.f, 0.3f, 0.3f, 1.f };
				float ChamsVisibleColor[4] = { 0.f, 1.f, 0.f, 0.7f };
				float ChamsHiddenColor[4] = { 1.f, 0.f, 0.f, 0.7f };

				float BoxColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float InvisibleVisibleBoxColor[4] = { 1.f, 0.f, 0.f, 1.f };
				float DistanceColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float SkeletonColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float InvisibleSkeletonColor[4] = { 1.f, 0.5f, 0.5f, 1.f };
				float FriendSkeletonColor[4] = { 0.f, 1.f, 1.f, 0.7f };
				float HealthBarColor[4] = { 0.f, 1.f, 0.f, 0.7f };
				float ArmorBarColor[4] = { 0.f, 0.f, 1.f, 0.7f };
				float TextColor[4] = { 1.f, 1.f, 1.f, 1.f };
				float HeadCircleColor[4] = { 0.8f, 0.8f, 0.8f, 0.12f };
			}Players;

			struct Vehicles
			{
				bool Enabled;
				bool Toogle;
				bool Distance;
				bool Name;
				bool ShowLockState;
				bool VisibleCheck;

				int ToggleKey = 0;
				int ToggleKeyState = 0;
				int EspOptions = 0;

				bool Marker;
				float MarkerColor[4] = { 0.5f, 0.5f, 0.5f, 0.7f };
				float DistanceColor[4] = { 0.6f, 0.6f, 0.6f, 0.7f };
				float NameColor[4] = { 0.7f, 0.7f, 0.7f, 0.7f };
			}Vehicles;

		}Visuals;

		struct Exploits
		{
			struct Self
			{
				bool Invisible;
				bool GodMode;
				bool NoClip;
				bool NoRagDoll;
				bool RemoveSpread;
				bool RemoveRecoil;
				bool SeatBelt;
				bool StealVehicle;
				bool UnLockAllVehicles;
				bool LockAllVehicles;
				bool ReplayTP;
				bool AntHS;
				bool AutoHeal = false;
				bool AutoArmor = false;
				bool VehicleBoost = false;
				bool VehicleRepair = false;
				bool ScreenshotCleaner = true;
				bool SpinBot = false;

				float WeaponSize = 1.f;
				float NoClipSpeed = 100.f;
				float AutoHealThreshold = 50.f;
				float AutoArmorThreshold = 50.f;
				float VehicleBoostMultiplier = 2.f;
				float SpinBotSpeed = 20.f;

				int NoClipKey = 0;
				int NoClipKeyState = 0;
				int GodKey = 0;
				int GodKeyState = 0;
				int ReplayTPBind = 0;
				int ReplayTPBindState = 0;
				int NoClipMode = 0;
				int VehicleBoostKey = 0;
				int VehicleBoostKeyState = 0;
				int VehicleRepairKey = 0;
				int VehicleRepairKeyState = 0;

#if defined(TRINITY_DEV) && TRINITY_DEV
				bool PlateChanger = false;
				bool PlateKeepApplied = false;
				bool PlateApplyPending = false;
				bool PlateUseBridge = true;
				char PlateText[9] = "TRINITY";
				char PlateCurrentPlate[9] = "";
				int PlateChangerKey = 0;
				int PlateChangerKeyState = 0;
#endif
			}Self;
			
			struct Session
			{
				bool ShowSessionInfo = false;
				int SessionStartTime = 0;
				int TotalKills = 0;
				int TotalDeaths = 0;
				int TotalHeadshots = 0;
			}Session;

		}Exploits;

		struct General
		{
			std::atomic<bool> ShutDown{ false };
			bool CaptureBypass = true;
			bool Vsync = true;
			bool SecondMonitor = false;
			bool IsAuthenticated = false;
			bool ShowWatermark = true;
			bool ShowFPS = false;
			bool ShowPing = false;
			bool ShowTime = true;
			bool ShowKeybindList = true; 
			bool CleanTraces = false; // Desabilitado por padrão
			int MenuKey = VK_RSHIFT;
			int MenuKeyState = 0;
			int MonitorIndex = -1; // Indice do monitor selecionado (-1 = auto)
			
			std::string Role = "User";
			std::string DiscordID = "";
			std::string DiscordUsername = "";
			std::string DiscordAvatarURL = "";
		}General;

		struct Crosshair
		{
			bool Enabled = false;
			bool ShowDot = true;
			bool ShowLines = true;
			bool ShowOutline = false;
			bool DynamicGap = false; // gap aumenta quando aimbot esta ativo

			int Style = 0;       // 0=Cross, 1=Circle, 2=Cross+Circle, 3=Dot only
			int Size = 8;        // comprimento das linhas
			int Gap = 4;         // espaco entre centro e linha
			int Thickness = 1;   // espessura das linhas
			int DotSize = 2;     // tamanho do ponto central
			int OutlineThickness = 1;
			int Rounding = 0;    // arredondamento das bordas (0-12)

			float Color[4]   = { 1.f, 1.f, 1.f, 1.f };
			float OutlineColor[4] = { 0.f, 0.f, 0.f, 1.f };
			float DotColor[4] = { 1.f, 1.f, 1.f, 1.f };
		}Crosshair;
	};
}

inline Cheat::Options g_Options;
