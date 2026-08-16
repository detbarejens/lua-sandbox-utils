#include "ObserverAlert.hpp"

#include "../../../Definations/Variables.hpp"
#include "../../../FiveM-External.hpp"
#include "../../../FivemSDK/GTADefines.hpp"
#include "../../../Utils/Misc.hpp"
#include "../../../Utils/VisibilityCheck.hpp"
#include "../../../ImGui/imgui.h"
#include "../../../ImGui/imgui_internal.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Cheat
{
	namespace ObserverAlert
	{
		namespace
		{
			struct ThreatScanResult
			{
				int count = 0;
				float closestDistance = 0.f;
				std::string topName;
				float topDistance = 0.f;
				float topLookPercent = 0.f;
				bool topIsThreat = false;
				bool hasTop = false;
			};

			using ObserverScanResult = ThreatScanResult;
			using AimingScanResult = ThreatScanResult;

			Vector3D Normalize(const Vector3D& value)
			{
				const float length = value.Length();
				if (length <= 0.0001f)
					return Vector3D(0.f, 0.f, 0.f);

				return Vector3D(value.x / length, value.y / length, value.z / length);
			}

			Vector3D GetPedForwardFromNavigation(CPed* ped)
			{
				if (!ped)
					return Vector3D(0.f, 1.f, 0.f);

				const uint64_t navigation = ped->GetNavigation();
				if (!navigation)
					return Vector3D(0.f, 1.f, 0.f);

				Vector3D forward(
					FrameWork::Memory::ReadMemory<float>(navigation + 0x20),
					FrameWork::Memory::ReadMemory<float>(navigation + 0x24),
					FrameWork::Memory::ReadMemory<float>(navigation + 0x28));

				if (forward.Length2D() <= 0.01f)
					return Vector3D(0.f, 1.f, 0.f);

				return Normalize(forward);
			}

			Vector3D GetPedLookForward(CPed* ped, const Entity& entity)
			{
				if (!ped)
					return Vector3D(0.f, 1.f, 0.f);

				const Matrix4x4 entityMatrix = FrameWork::Memory::ReadMemory<Matrix4x4>(reinterpret_cast<uintptr_t>(ped) + 0x60);
				Vector3D matrixForward(entityMatrix._21, entityMatrix._22, entityMatrix._23);
				if (matrixForward.Length2D() > 0.01f)
					return Normalize(matrixForward);

				if (ped->HasConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) || ped->HasConfigFlag(CPED_CONFIG_FLAG_ForcedAim))
				{
					const Vector3D head = g_Fivem.GetBonePosVec3(entity, SKEL_Head);
					const Vector3D chest = g_Fivem.GetBonePosVec3(entity, SKEL_Spine3);
					if (!head.IsZero() && !chest.IsZero())
					{
						Vector3D aimHint(
							head.x - chest.x,
							head.y - chest.y,
							(head.z - chest.z) * 0.5f);
						if (aimHint.Length2D() > 0.01f)
							return Normalize(aimHint);
					}
				}

				return GetPedForwardFromNavigation(ped);
			}

			Vector3D GetLocalTargetPosition()
			{
				Vector3D headPos;
				Vector3D chestPos;
				bool hasHead = false;
				bool hasChest = false;

				for (const Entity& entity : g_Fivem.GetEntitiyList())
				{
					if (!entity.StaticInfo.bIsLocalPlayer)
						continue;

					headPos = g_Fivem.GetBonePosVec3(entity, SKEL_Head);
					chestPos = g_Fivem.GetBonePosVec3(entity, SKEL_Spine3);
					hasHead = !headPos.IsZero();
					hasChest = !chestPos.IsZero();
					if (!hasHead)
						headPos = entity.Cordinates;
					break;
				}

				if (hasHead && hasChest)
				{
					return Vector3D(
						headPos.x * 0.55f + chestPos.x * 0.45f,
						headPos.y * 0.55f + chestPos.y * 0.45f,
						headPos.z * 0.55f + chestPos.z * 0.45f);
				}

				if (hasHead)
					return headPos;

				const auto localInfo = g_Fivem.GetLocalPlayerInfo();
				return localInfo.WorldPos;
			}

			float ComputeLookDot(const Vector3D& observerEye, const Vector3D& lookForward, const Vector3D& targetPos)
			{
				const Vector3D toTarget = Normalize(Vector3D(
					targetPos.x - observerEye.x,
					targetPos.y - observerEye.y,
					targetPos.z - observerEye.z));

				return lookForward.x * toTarget.x + lookForward.y * toTarget.y + lookForward.z * toTarget.z;
			}

			bool IsObserverCandidate(const Entity& entity, CPed* localPed)
			{
				if (!entity.StaticInfo.Ped || entity.StaticInfo.bIsLocalPlayer)
					return false;
				if (entity.StaticInfo.bIsNPC)
					return false;
				if (entity.StaticInfo.IsFriend)
					return false;
				if (g_Options.Visuals.Players.ExcludeDeads && entity.StaticInfo.Ped->GetHealth() <= 0.f)
					return false;

				return entity.StaticInfo.Ped != localPed;
			}

			std::string GetEntityDisplayName(const Entity& entity)
			{
				if (!entity.NetworkInfo.UserName.empty() && entity.NetworkInfo.UserName != "Unknown")
					return entity.NetworkInfo.UserName;
				if (!entity.StaticInfo.Name.empty() && entity.StaticInfo.Name != "NPC")
					return entity.StaticInfo.Name;
				return "Unknown";
			}

			ThreatScanResult ScanThreats(bool aimingOnly)
			{
				ThreatScanResult result;
				const auto localInfo = g_Fivem.GetLocalPlayerInfo();
				if (!localInfo.Ped)
					return result;

				const Vector3D localPos = GetLocalTargetPosition();
				const int maxDistance = aimingOnly
					? g_Options.Visuals.Players.AimingMaxDistance
					: g_Options.Visuals.Players.ObserverMaxDistance;
				const int awarenessFov = aimingOnly
					? g_Options.Visuals.Players.AimingAwarenessFov
					: g_Options.Visuals.Players.ObserverCanSeeFov;
				const int threatThreshold = aimingOnly
					? g_Options.Visuals.Players.AimingLookThreshold
					: g_Options.Visuals.Players.ObserverLookThreshold;
				const bool visibleCheck = aimingOnly
					? g_Options.Visuals.Players.AimingVisibleCheck
					: g_Options.Visuals.Players.ObserverVisibleCheck;

				const float awarenessDot = cosf(awarenessFov * (3.14159265358979323846f / 180.f) * 0.5f);
				const float threatDot = static_cast<float>(threatThreshold) / 100.f;

				for (const Entity& entity : g_Fivem.GetEntitiyList())
				{
					if (!IsObserverCandidate(entity, localInfo.Ped))
						continue;

					CPed* ped = entity.StaticInfo.Ped;
					if (aimingOnly &&
						!ped->HasConfigFlag(CPED_CONFIG_FLAG_IsAimingGun) &&
						!ped->HasConfigFlag(CPED_CONFIG_FLAG_ForcedAim))
					{
						continue;
					}

					Vector3D observerEye = g_Fivem.GetBonePosVec3(entity, SKEL_Head);
					if (observerEye.IsZero())
						observerEye = entity.Cordinates;

					const float distance = observerEye.DistTo(localPos);
					if (distance > static_cast<float>(maxDistance) || distance < 1.f)
						continue;

					const Vector3D lookForward = GetPedLookForward(ped, entity);
					const float lookDot = ComputeLookDot(observerEye, lookForward, localPos);
					if (lookDot < awarenessDot)
						continue;

					if (visibleCheck)
					{
						Entity visibleEntity = entity;
						if (!FrameWork::Visibility::IsPlayerVisible(visibleEntity, g_Options.Visuals.Players.VisiblePixelThreshold))
							continue;
					}

					++result.count;
					if (result.closestDistance <= 0.f || distance < result.closestDistance)
						result.closestDistance = distance;

					const float lookPercent = std::clamp(lookDot, 0.f, 1.f) * 100.f;
					if (!result.hasTop || lookPercent > result.topLookPercent)
					{
						result.hasTop = true;
						result.topName = GetEntityDisplayName(entity);
						result.topDistance = distance;
						result.topLookPercent = lookPercent;
						result.topIsThreat = lookDot >= threatDot;
					}
				}

				return result;
			}

			ObserverScanResult ScanObservers()
			{
				return ScanThreats(false);
			}

			AimingScanResult ScanAimers()
			{
				return ScanThreats(true);
			}

			ImFont* ResolveFont()
			{
				if (FrameWork::Assets::InterBold && FrameWork::Assets::InterBold->IsLoaded())
					return FrameWork::Assets::InterBold;

				ImGuiIO& io = ImGui::GetIO();
				if (io.Fonts && !io.Fonts->Fonts.empty())
					return io.Fonts->Fonts[0];

				return ImGui::GetFont();
			}

			void DrawBellIcon(ImDrawList* drawList, ImVec2 center, ImU32 color)
			{
				drawList->AddCircle(ImVec2(center.x, center.y - 1.f), 7.f, color, 20, 1.5f);
				drawList->AddLine(ImVec2(center.x, center.y + 6.f), ImVec2(center.x, center.y + 10.f), color, 1.5f);
				drawList->AddLine(ImVec2(center.x - 4.f, center.y + 10.f), ImVec2(center.x + 4.f, center.y + 10.f), color, 1.5f);
			}

			void DrawCrosshairIcon(ImDrawList* drawList, ImVec2 center, ImU32 color)
			{
				drawList->AddCircle(center, 7.f, color, 20, 1.6f);
				drawList->AddLine(ImVec2(center.x - 9.f, center.y), ImVec2(center.x - 3.f, center.y), color, 1.6f);
				drawList->AddLine(ImVec2(center.x + 3.f, center.y), ImVec2(center.x + 9.f, center.y), color, 1.6f);
				drawList->AddLine(ImVec2(center.x, center.y - 9.f), ImVec2(center.x, center.y - 3.f), color, 1.6f);
				drawList->AddLine(ImVec2(center.x, center.y + 3.f), ImVec2(center.x, center.y + 9.f), color, 1.6f);
			}

			void DrawWarningIcon(ImDrawList* drawList, ImVec2 center, ImU32 color)
			{
				const ImVec2 p1(center.x, center.y - 8.f);
				const ImVec2 p2(center.x - 8.f, center.y + 7.f);
				const ImVec2 p3(center.x + 8.f, center.y + 7.f);
				drawList->AddTriangle(p1, p2, p3, color, 1.8f);
				drawList->AddLine(ImVec2(center.x, center.y - 2.f), ImVec2(center.x, center.y + 2.f), color, 1.8f);
				drawList->AddCircleFilled(ImVec2(center.x, center.y + 5.f), 1.3f, color, 8);
			}

			enum class HudIconType
			{
				Bell,
				Warning,
				Crosshair
			};

			void DrawHudIcon(ImDrawList* drawList, ImVec2 center, HudIconType iconType, ImU32 color)
			{
				switch (iconType)
				{
				case HudIconType::Warning:
					DrawWarningIcon(drawList, center, color);
					break;
				case HudIconType::Crosshair:
					DrawCrosshairIcon(drawList, center, color);
					break;
				case HudIconType::Bell:
				default:
					DrawBellIcon(drawList, center, color);
					break;
				}
			}

			struct ObserverHudLayout
			{
				ImVec2 boxMin;
				ImVec2 boxMax;
				ImRect headerRect;
				ImRect closeRect;
			};

			struct ObserverHudMetrics
			{
				ImVec2 size;
			};

			ObserverHudMetrics MeasureHudPanel(const char* countText, const char* detailText, bool menuOpen)
			{
				ImFont* font = ResolveFont();
				const float fontSize = 16.f;
				const float headerHeight = menuOpen ? 22.f : 0.f;
				const ImVec2 padding(14.f, 8.f);
				const float iconSpace = 28.f;
				const float contentWidth = ImMax(
					FrameWork::Misc::CalcTextSize(font, (int)fontSize, countText).x,
					detailText ? FrameWork::Misc::CalcTextSize(font, (int)fontSize, detailText).x : 0.f);
				const float bodyHeight = detailText ? (fontSize * 2.f + 10.f) : fontSize;
				const float totalHeight = headerHeight + bodyHeight + padding.y * 2.f;
				return { ImVec2(contentWidth + padding.x * 2.f + iconSpace, ImMax(totalHeight, 34.f)) };
			}

			ObserverHudLayout BuildHudLayout(const ImVec2& topLeft, const ImVec2& panelSize)
			{
				ObserverHudLayout layout;
				layout.boxMin = topLeft;
				layout.boxMax = ImVec2(topLeft.x + panelSize.x, topLeft.y + panelSize.y);
				layout.headerRect = ImRect(layout.boxMin, ImVec2(layout.boxMax.x, layout.boxMin.y + 22.f));
				layout.closeRect = ImRect(
					ImVec2(layout.boxMax.x - 22.f, layout.boxMin.y),
					ImVec2(layout.boxMax.x, layout.boxMin.y + 22.f));
				return layout;
			}

			void DrawHudPanel(const ObserverHudLayout& layout, const char* countText, const char* detailText,
				bool warning, bool menuOpen, const char* headerTitle, HudIconType idleIcon, HudIconType warningIcon)
			{
				ImDrawList* drawList = ImGui::GetForegroundDrawList();
				ImFont* font = ResolveFont();
				if (!drawList || !font || !countText)
					return;

				const float fontSize = 16.f;
				const float headerHeight = menuOpen ? 22.f : 0.f;
				const float iconSpace = 28.f;
				const ImVec2 padding(14.f, 8.f);
				const float boxHeight = layout.boxMax.y - layout.boxMin.y;

				drawList->AddRectFilled(layout.boxMin, layout.boxMax, IM_COL32(18, 18, 20, 65), 8.f);
				drawList->AddRect(layout.boxMin, layout.boxMax, IM_COL32(255, 255, 255, 6), 8.f);

				if (menuOpen)
				{
					drawList->AddLine(
						ImVec2(layout.boxMin.x + 8.f, layout.boxMin.y + headerHeight),
						ImVec2(layout.boxMax.x - 8.f, layout.boxMin.y + headerHeight),
						IM_COL32(255, 255, 255, 16), 1.f);

					drawList->AddText(font, 13.f, ImVec2(layout.boxMin.x + 10.f, layout.boxMin.y + 4.f),
						IM_COL32(170, 170, 170, 255), headerTitle);

					const ImU32 closeColor = IM_COL32(220, 90, 90, 255);
					const ImVec2 closeCenter(
						(layout.closeRect.Min.x + layout.closeRect.Max.x) * 0.5f,
						(layout.closeRect.Min.y + layout.closeRect.Max.y) * 0.5f);
					drawList->AddLine(
						ImVec2(closeCenter.x - 4.f, closeCenter.y - 4.f),
						ImVec2(closeCenter.x + 4.f, closeCenter.y + 4.f),
						closeColor, 1.6f);
					drawList->AddLine(
						ImVec2(closeCenter.x + 4.f, closeCenter.y - 4.f),
						ImVec2(closeCenter.x - 4.f, closeCenter.y + 4.f),
						closeColor, 1.6f);
				}

				const ImU32 iconColor = warning ? IM_COL32(255, 210, 70, 255) : IM_COL32(210, 210, 210, 255);
				const float contentTop = layout.boxMin.y + headerHeight + padding.y;
				const float iconCenterY = menuOpen
					? (contentTop + 8.f)
					: (layout.boxMin.y + boxHeight * 0.5f);
				const ImVec2 iconCenter(layout.boxMin.x + 18.f, iconCenterY);
				DrawHudIcon(drawList, iconCenter, warning ? warningIcon : idleIcon, iconColor);

				const float textX = layout.boxMin.x + iconSpace + 4.f;
				const float countY = menuOpen
					? contentTop
					: (layout.boxMin.y + (boxHeight - (detailText && detailText[0] ? fontSize * 2.f + 4.f : fontSize)) * 0.5f);
				drawList->AddText(font, fontSize, ImVec2(textX, countY), IM_COL32(220, 220, 220, 255), countText);

				if (detailText && detailText[0] != '\0')
				{
					const ImU32 detailColor = warning ? IM_COL32(255, 210, 70, 255) : IM_COL32(190, 190, 190, 255);
					drawList->AddText(font, fontSize, ImVec2(textX, countY + fontSize + 4.f), detailColor, detailText);
				}
			}

			ImVec2 ResolveHudTopLeft(const ImVec2& panelSize, const ImVec2& screen, float hudX, float hudY, float defaultY)
			{
				float x = hudX;
				float y = hudY;

				if (x < 0.f || y < 0.f)
				{
					x = 0.5f;
					y = defaultY;
				}

				ImVec2 topLeft(x * screen.x - panelSize.x * 0.5f, y * screen.y - panelSize.y * 0.5f);
				topLeft.x = std::clamp(topLeft.x, 8.f, ImMax(8.f, screen.x - panelSize.x - 8.f));
				topLeft.y = std::clamp(topLeft.y, 8.f, ImMax(8.f, screen.y - panelSize.y - 8.f));
				return topLeft;
			}

			void PersistHudPosition(const ImVec2& topLeft, const ImVec2& panelSize, const ImVec2& screen, float& hudX, float& hudY)
			{
				const ImVec2 anchor(topLeft.x + panelSize.x * 0.5f, topLeft.y + panelSize.y * 0.5f);
				hudX = anchor.x / screen.x;
				hudY = anchor.y / screen.y;
			}

			bool HandleHudInteraction(ImVec2& topLeft, const ObserverHudLayout& layout, const ImVec2& panelSize,
				const ImVec2& screen, bool menuOpen, bool& showHud, float& hudX, float& hudY)
			{
				if (!menuOpen)
					return false;

				ImGuiIO& io = ImGui::GetIO();
				static bool observerDragging = false;
				static bool aimingDragging = false;
				static ImVec2 observerDragOffset = ImVec2(0.f, 0.f);
				static ImVec2 aimingDragOffset = ImVec2(0.f, 0.f);
				const bool isAimingHud = &showHud == &g_Options.Visuals.Players.AimingAlertShowHud;
				bool& dragging = isAimingHud ? aimingDragging : observerDragging;
				ImVec2& dragOffset = isAimingHud ? aimingDragOffset : observerDragOffset;

				const ImVec2 mouse = io.MousePos;
				const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
				const bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

				if (mouseClicked && layout.closeRect.Contains(mouse))
				{
					showHud = false;
					dragging = false;
					return true;
				}

				if (mouseClicked && layout.headerRect.Contains(mouse) && !layout.closeRect.Contains(mouse))
				{
					dragging = true;
					dragOffset = mouse - topLeft;
				}

				if (!mouseDown)
					dragging = false;

				if (dragging)
				{
					topLeft = mouse - dragOffset;
					topLeft.x = std::clamp(topLeft.x, 8.f, ImMax(8.f, screen.x - panelSize.x - 8.f));
					topLeft.y = std::clamp(topLeft.y, 8.f, ImMax(8.f, screen.y - panelSize.y - 8.f));
					PersistHudPosition(topLeft, panelSize, screen, hudX, hudY);
					return true;
				}

				return false;
			}

			void RenderThreatPanel(const ThreatScanResult& scan, bool menuOpen, const char* headerTitle,
				const char* countLabel, const char* threatVerb, const char* awareVerb,
				float& hudX, float& hudY, bool& showHud, float defaultY,
				HudIconType idleIcon, HudIconType warningIcon)
			{
				const ImVec2 screen = ImGui::GetIO().DisplaySize;

				char countText[128];
				if (scan.count > 0)
					snprintf(countText, sizeof(countText), "%d %s [%.0fm]", scan.count, countLabel, scan.closestDistance);
				else
					snprintf(countText, sizeof(countText), "0 %s", countLabel);

				char detailText[160]{};
				const char* detailPtr = nullptr;
				bool warning = false;
				if (scan.hasTop && scan.count > 0)
				{
					if (scan.topIsThreat)
					{
						snprintf(detailText, sizeof(detailText), "%s %s [%.0f%%] [%.0fm]",
							scan.topName.c_str(), threatVerb, scan.topLookPercent, scan.topDistance);
					}
					else
					{
						snprintf(detailText, sizeof(detailText), "%s %s [%.0f%%] [%.0fm]",
							scan.topName.c_str(), awareVerb, scan.topLookPercent, scan.topDistance);
					}
					detailPtr = detailText;
					warning = scan.topIsThreat;
				}

				const ObserverHudMetrics metrics = MeasureHudPanel(countText, detailPtr, menuOpen);
				const ImVec2 panelSize = metrics.size;
				ImVec2 topLeft = ResolveHudTopLeft(panelSize, screen, hudX, hudY, defaultY);
				ObserverHudLayout layout = BuildHudLayout(topLeft, panelSize);
				HandleHudInteraction(topLeft, layout, panelSize, screen, menuOpen, showHud, hudX, hudY);
				layout = BuildHudLayout(topLeft, panelSize);
				DrawHudPanel(layout, countText, detailPtr, warning, menuOpen, headerTitle, idleIcon, warningIcon);
			}
		}

		void Render(bool menuOpen)
		{
			if (!g_Options.Visuals.Players.ObserverAlert)
				return;

			if (!g_Options.Visuals.Players.ObserverAlertShowHud)
				return;

			if (!g_Fivem.IsInitialized())
				return;

			const ObserverScanResult scan = ScanObservers();
			RenderThreatPanel(scan, menuOpen, "Observer Alert - drag",
				"nearby observers", "is looking at you", "can see you",
				g_Options.Visuals.Players.ObserverAlertHudX,
				g_Options.Visuals.Players.ObserverAlertHudY,
				g_Options.Visuals.Players.ObserverAlertShowHud,
				0.56f, HudIconType::Bell, HudIconType::Warning);
		}

		void RenderAimingAlert(bool menuOpen)
		{
			if (!g_Options.Visuals.Players.AimingAlert)
				return;

			if (!g_Options.Visuals.Players.AimingAlertShowHud)
				return;

			if (!g_Fivem.IsInitialized())
				return;

			const AimingScanResult scan = ScanAimers();
			RenderThreatPanel(scan, menuOpen, "Aiming Alert - drag",
				"players aiming", "is aiming at you", "is aiming nearby",
				g_Options.Visuals.Players.AimingAlertHudX,
				g_Options.Visuals.Players.AimingAlertHudY,
				g_Options.Visuals.Players.AimingAlertShowHud,
				0.64f, HudIconType::Crosshair, HudIconType::Warning);
		}
	}
}
