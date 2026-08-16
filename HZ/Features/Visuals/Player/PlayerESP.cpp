#include "PlayerESP.hpp"

#include <cmath>
#include <minmax.h>
#include <unordered_map>
#include <vector>

#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#include "../../../FiveM-External.hpp"
#include "../../../Definations/Variables.hpp"
#include "../../../Utils/VisibilityCheck.hpp"

namespace Cheat
{
	float clamp(float val, float minVal, float maxVal) {
		return (val < minVal) ? minVal : (val > maxVal ? maxVal : val);
	}

	void ESP::Players()
	{
		if (!g_Options.Visuals.Players.Enabled)
			return;

		ImDrawList* DrawList = ImGui::GetBackgroundDrawList();
		if (!DrawList)
			return;

		try
		{
			// Additional safety check for entity list
			auto entityList = g_Fivem.GetEntitiyList();
			if (entityList.empty())
				return;
				
			for (Entity Current : entityList)
			{
				// Safety checks
				if (!Current.StaticInfo.Ped)
					continue;
					
				if (Current.StaticInfo.bIsLocalPlayer && !g_Options.Visuals.Players.ShowLocalPlayer)
					continue;
				if (Current.StaticInfo.bIsNPC && !g_Options.Visuals.Players.ShowNPC)
					continue;

				try
				{
					Vector3D PedCoordinates = Current.StaticInfo.Ped->GetCoordinate();
					if (PedCoordinates.IsZero())
						continue;
						
					float Distance = PedCoordinates.DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos);
					if (Distance > g_Options.Visuals.Players.RenderDistance)
						continue;

					ImVec2 PedLocation = g_Fivem.WorldToScreen(PedCoordinates);
					if (!g_Fivem.IsOnScreen(PedLocation))
						continue;

					ImVec2 Head = Current.HeadPos;
					if (!g_Fivem.IsOnScreen(Head))
						continue;

					ImVec2 LeftFoot = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_L_Foot));
					ImVec2 RightFoot = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, SKEL_R_Foot));
					if (!g_Fivem.IsOnScreen(LeftFoot) || !g_Fivem.IsOnScreen(RightFoot))
						continue;

					float Height = -Head.y + (LeftFoot.y > RightFoot.y ? LeftFoot.y : RightFoot.y);
					float Width = Height / 1.8f;
					float PedCenterY = Head.y + Height / 2.f;
					Height *= 1.2f;

					ImVec2 Padding[4] = { ImVec2(0,0), ImVec2(0,0), ImVec2(0,0), ImVec2(0,0) };

			// Visible check
			bool isVisible = true;
			if (g_Options.Visuals.Players.VisibleCheck)
				isVisible = FrameWork::Visibility::IsPlayerVisible(Current, g_Options.Visuals.Players.VisiblePixelThreshold);
			Current.Visible = isVisible;

			ImColor CheckBoxColor = isVisible
				? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.BoxColor)
				: FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.InvisibleVisibleBoxColor);

			// Box
			if (g_Options.Visuals.Players.EnableBox)
			{
				if (g_Options.Visuals.Players.BoxType == 0)
				{
					DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.BoxColor[3]), 0, ImDrawFlags_None, 3);
					DrawList->AddRect(ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2.f), ImVec2(PedLocation.x + Width / 2, PedCenterY + Height / 2.f), CheckBoxColor, 0, ImDrawFlags_None, 2);
					Padding[0].y += 3; Padding[1].x += 3; Padding[2].y += 3; Padding[3].x += 3;
				}
				else if (g_Options.Visuals.Players.BoxType == 1)
				{
					float cs = ((Height / 2.f / 100.f) + (Width / 2.f / 100.f)) / 2.f * 30.f;
					ImVec2 TL[] = { {PedLocation.x - Width / 2.f, PedLocation.y - Height / 2.f + cs},{PedLocation.x - Width / 2.f,PedLocation.y - Height / 2.f},{PedLocation.x - Width / 2.f + cs,PedLocation.y - Height / 2.f} };
					ImVec2 TR[] = { {PedLocation.x + Width / 2.f, PedLocation.y - Height / 2.f + cs},{PedLocation.x + Width / 2.f,PedLocation.y - Height / 2.f},{PedLocation.x + Width / 2.f - cs,PedLocation.y - Height / 2.f} };
					ImVec2 BL[] = { {PedLocation.x - Width / 2.f, PedLocation.y + Height / 2.f - cs},{PedLocation.x - Width / 2.f,PedLocation.y + Height / 2.f},{PedLocation.x - Width / 2.f + cs,PedLocation.y + Height / 2.f} };
					ImVec2 BR[] = { {PedLocation.x + Width / 2.f, PedLocation.y + Height / 2.f - cs},{PedLocation.x + Width / 2.f,PedLocation.y + Height / 2.f},{PedLocation.x + Width / 2.f - cs,PedLocation.y + Height / 2.f} };
					DrawList->AddPolyline(TL, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3); DrawList->AddPolyline(TL, 3, CheckBoxColor, ImDrawFlags_None, 2);
					DrawList->AddPolyline(TR, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3); DrawList->AddPolyline(TR, 3, CheckBoxColor, ImDrawFlags_None, 2);
					DrawList->AddPolyline(BL, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3); DrawList->AddPolyline(BL, 3, CheckBoxColor, ImDrawFlags_None, 2);
					DrawList->AddPolyline(BR, 3, ImColor(0.f, 0.f, 0.f, 1.f), ImDrawFlags_None, 3); DrawList->AddPolyline(BR, 3, CheckBoxColor, ImDrawFlags_None, 2);
					Padding[0].y += 3; Padding[1].x += 3; Padding[2].y += 3; Padding[3].x += 3;
				}
			}

			// Radar
			if (g_Options.Visuals.Players.RadarEnabled)
			{
				const float radarSize = 200.f;
				const ImVec2 radarPos(50.f, 50.f);
				const float scale = radarSize / (2.f * g_Options.Visuals.Players.RenderDistance);
				DrawList->AddRectFilled(radarPos, ImVec2(radarPos.x + radarSize, radarPos.y + radarSize), ImColor(0, 0, 0, 150), 5.f);
				DrawList->AddRect(radarPos, ImVec2(radarPos.x + radarSize, radarPos.y + radarSize), ImColor(255, 255, 255, 200), 5.f, 0, 1.5f);
				ImVec2 rc(radarPos.x + radarSize * 0.5f, radarPos.y + radarSize * 0.5f);
				DrawList->AddCircleFilled(rc, 3.f, ImColor(255, 255, 0, 255), 12);
				Vector3D lp = g_Fivem.GetLocalPlayerInfo().WorldPos;
				for (auto& ent : g_Fivem.GetEntitiyList())
				{
					if (!g_Options.Visuals.Players.ShowLocalPlayer && ent.StaticInfo.bIsLocalPlayer) continue;
					if (!g_Options.Visuals.Players.ShowNPC && ent.StaticInfo.bIsNPC) continue;
					if (g_Options.Visuals.Players.VisibleCheck && !ent.StaticInfo.Ped->IsVisible()) continue;
					Vector3D ep = ent.StaticInfo.Ped->GetCoordinate();
					if (ep.DistTo(lp) > g_Options.Visuals.Players.RenderDistance) continue;
					ImVec2 rp(rc.x + (ep.x - lp.x) * scale, rc.y - (ep.y - lp.y) * scale);
					rp.x = clamp(rp.x, radarPos.x, radarPos.x + radarSize);
					rp.y = clamp(rp.y, radarPos.y, radarPos.y + radarSize);
					DrawList->AddCircleFilled(rp, 3.f, ImColor(255, 0, 0, 255), 12);
				}
			}

			// LineToHead
			if (g_Options.Visuals.Players.LineToHead && (Head.x != 0.f || Head.y != 0.f))
			{
				ImVec2 sc(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
				DrawList->AddLine(sc, Head, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.SnapLineColor), 1.f);
			}

			// CrosshairOnTarget
			if (g_Options.Visuals.Players.CrosshairOnTarget)
			{
				ImVec2 sc(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
				if (Head.DistTo(sc) < 60.f)
				{
					float sz = 10.f, gap = 4.f;
					ImColor col(255, 50, 50, 220);
					DrawList->AddLine(ImVec2(sc.x - sz - gap, sc.y), ImVec2(sc.x - gap, sc.y), col, 1.5f);
					DrawList->AddLine(ImVec2(sc.x + gap, sc.y), ImVec2(sc.x + sz + gap, sc.y), col, 1.5f);
					DrawList->AddLine(ImVec2(sc.x, sc.y - sz - gap), ImVec2(sc.x, sc.y - gap), col, 1.5f);
					DrawList->AddLine(ImVec2(sc.x, sc.y + gap), ImVec2(sc.x, sc.y + sz + gap), col, 1.5f);
				}
			}

			// PulseCircle
			if (g_Options.Visuals.Players.PulseCircle)
			{
				static float pt = 0.f; pt += ImGui::GetIO().DeltaTime * 2.5f;
				float pulse = (sinf(pt) + 1.f) * 0.5f;
				ImColor col = Current.StaticInfo.IsFriend ? ImColor(0.f, 1.f, 1.f, 0.9f - pulse * 0.5f) : ImColor(1.f, 0.2f + pulse * 0.3f, 0.2f, 0.9f - pulse * 0.5f);
				DrawList->AddCircle(Head, 8.f + pulse * 12.f, col, 32, 1.5f);
			}

			// RainbowSkeleton
			if (g_Options.Visuals.Players.RainbowSkeleton)
			{
				static float rbt = 0.f; rbt += ImGui::GetIO().DeltaTime * 1.5f;
				float off = Current.StaticInfo.iIndex * 0.4f;
				ImColor rbCol((sinf(rbt + off) + 1.f) * 0.5f, (sinf(rbt + off + 2.094f) + 1.f) * 0.5f, (sinf(rbt + off + 4.189f) + 1.f) * 0.5f, 1.f);
				uint64_t frag = FrameWork::Memory::ReadMemory<uint64_t>((uintptr_t)Current.StaticInfo.Ped + Cheat::Offsets::FragInsNmGTA);
				if (frag) Current.StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(frag + 0x68) + 0x178);
				auto RB = [&](unsigned int a, unsigned int b) {
					ImVec2 pa = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, a));
					ImVec2 pb = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, b));
					if ((pa.x != 0.f || pa.y != 0.f) && (pb.x != 0.f || pb.y != 0.f)) DrawList->AddLine(pa, pb, rbCol, 2.f);
					};
				RB(SKEL_Neck_1, SKEL_R_Clavicle); RB(SKEL_Neck_1, SKEL_L_Clavicle);
				RB(SKEL_R_Clavicle, SKEL_R_UpperArm); RB(SKEL_L_Clavicle, SKEL_L_UpperArm);
				RB(SKEL_R_UpperArm, SKEL_R_Forearm); RB(SKEL_L_UpperArm, SKEL_L_Forearm);
				RB(SKEL_R_Forearm, SKEL_R_Hand); RB(SKEL_L_Forearm, SKEL_L_Hand);
				RB(SKEL_Neck_1, SKEL_Pelvis);
				RB(SKEL_Pelvis, SKEL_L_Thigh); RB(SKEL_Pelvis, SKEL_R_Thigh);
				RB(SKEL_L_Thigh, SKEL_L_Calf); RB(SKEL_R_Thigh, SKEL_R_Calf);
				RB(SKEL_L_Calf, SKEL_L_Foot); RB(SKEL_R_Calf, SKEL_R_Foot);
			}

			// SpinningRing
			if (g_Options.Visuals.Players.SpinningRing)
			{
				static float spinT = 0.f; spinT += ImGui::GetIO().DeltaTime * 2.f;
				float radius = Width * 0.6f;
				for (int d = 0; d < 12; d++)
				{
					float angle = spinT + (d / 12.f) * IM_PI * 2.f;
					ImVec2 p(Head.x + cosf(angle) * radius, Head.y + sinf(angle) * radius * 0.4f);
					DrawList->AddCircleFilled(p, 2.5f, ImColor(0.6f, 0.f, 1.f, (float)d / 12.f), 8);
				}
			}

			// BoxBreath
			if (g_Options.Visuals.Players.BoxBreath)
			{
				static float breathT = 0.f; breathT += ImGui::GetIO().DeltaTime * 1.8f;
				float expand = sinf(breathT + Current.StaticInfo.iIndex * 0.5f) * 3.f;
				DrawList->AddRect(
					ImVec2(PedLocation.x - Width / 2 - expand, PedCenterY - Height / 2.f - expand),
					ImVec2(PedLocation.x + Width / 2 + expand, PedCenterY + Height / 2.f + expand),
					FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.BoxColor), 0, ImDrawFlags_None, 1.5f);
			}

			// HealthBar3D
			if (g_Options.Visuals.Players.HealthBar3D)
			{
				float hp = Current.StaticInfo.Ped->GetHealth(), maxHp = Current.StaticInfo.Ped->GetMaxHealth();
				float t = (maxHp > 0) ? (hp / maxHp) : 0.f; t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
				float bx = PedLocation.x - Width / 2 - 8.f, by = PedCenterY + Height / 2.f;
				DrawList->AddRectFilled(ImVec2(bx, by - Height), ImVec2(bx + 4.f, by), ImColor(0.15f, 0.15f, 0.15f, 0.8f));
				for (int s = 0; s < 20 && s < (int)(t * 20); s++)
				{
					float st = (float)s / 20;
					DrawList->AddRectFilled(ImVec2(bx, by - (st + 0.05f) * Height), ImVec2(bx + 4.f, by - st * Height), ImColor(1.f - st, st, 0.f, 0.9f));
				}
			}

			// EnemyArrow
			if (g_Options.Visuals.Players.EnemyArrow)
			{
				ImVec2 sc(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
				ImVec2 dir(PedLocation.x - sc.x, PedLocation.y - sc.y);
				float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
				if (len > 80.f)
				{
					dir.x /= len; dir.y /= len;
					ImVec2 base(sc.x + dir.x * 60.f, sc.y + dir.y * 60.f);
					ImVec2 tip(base.x + dir.x * 12.f, base.y + dir.y * 12.f);
					ImVec2 perp(-dir.y * 5.f, dir.x * 5.f);
					ImColor ac = Current.StaticInfo.IsFriend ? ImColor(0.f, 1.f, 1.f, 0.8f) : ImColor(1.f, 0.2f, 0.2f, 0.8f);
					DrawList->AddTriangleFilled(tip, ImVec2(base.x - perp.x, base.y - perp.y), ImVec2(base.x + perp.x, base.y + perp.y), ac);
				}
			}

			// DistanceRings
			if (g_Options.Visuals.Players.DistanceRings)
			{
				float t2 = Distance / (float)g_Options.Visuals.Players.RenderDistance;
				int rings = 3 - (int)(t2 * 3.f);
				for (int r = 1; r <= rings; r++)
					DrawList->AddCircle(PedLocation, Width * 0.4f * r, ImColor(0.8f, 0.8f, 1.f, 0.4f / r), 32, 1.f);
			}

			// ScanLine
			if (g_Options.Visuals.Players.ScanLine)
			{
				static float scanT = 0.f; scanT += ImGui::GetIO().DeltaTime * 0.8f;
				float cycle = fmodf(scanT + Current.StaticInfo.iIndex * 0.3f, 1.f);
				float scanY = (PedCenterY - Height / 2.f) + cycle * Height;
				float alpha = 1.f - fabsf(cycle - 0.5f) * 2.f;
				DrawList->AddLine(ImVec2(PedLocation.x - Width / 2, scanY), ImVec2(PedLocation.x + Width / 2, scanY), ImColor(0.f, 1.f, 0.5f, alpha * 0.7f), 1.5f);
			}

			// Skeleton
			if (g_Options.Visuals.Players.Skeleton)
			{
				ImColor BaseColor = Current.StaticInfo.IsFriend
					? FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.FriendSkeletonColor)
					: FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.SkeletonColor);
				ImColor Color = g_Options.Visuals.Players.VisibleCheck
					? (isVisible ? ImColor(0, 255, 0, 255) : ImColor(255, 0, 0, 255))
					: BaseColor;

				uint64_t frag2 = FrameWork::Memory::ReadMemory<uint64_t>((uintptr_t)Current.StaticInfo.Ped + Cheat::Offsets::FragInsNmGTA);
				if (frag2) Current.StaticInfo.crSkeletonData = FrameWork::Memory::ReadMemory<uint64_t>(FrameWork::Memory::ReadMemory<uint64_t>(frag2 + 0x68) + 0x178);

				auto Bone = [&](unsigned int a, unsigned int b) {
					ImVec2 pa = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, a));
					ImVec2 pb = g_Fivem.WorldToScreen(g_Fivem.GetBonePosVec3(Current, b));
					if ((pa.x != 0.f || pa.y != 0.f) && (pb.x != 0.f || pb.y != 0.f)) DrawList->AddLine(pa, pb, Color, 1.f);
					};
				Bone(SKEL_Neck_1, SKEL_R_Clavicle); Bone(SKEL_Neck_1, SKEL_L_Clavicle);
				Bone(SKEL_R_Clavicle, SKEL_R_UpperArm); Bone(SKEL_L_Clavicle, SKEL_L_UpperArm);
				Bone(SKEL_R_UpperArm, SKEL_R_Forearm); Bone(SKEL_L_UpperArm, SKEL_L_Forearm);
				Bone(SKEL_R_Forearm, SKEL_R_Hand); Bone(SKEL_L_Forearm, SKEL_L_Hand);
				Bone(SKEL_Neck_1, SKEL_Pelvis);
				Bone(SKEL_Pelvis, SKEL_L_Thigh); Bone(SKEL_Pelvis, SKEL_R_Thigh);
				Bone(SKEL_L_Thigh, SKEL_L_Calf); Bone(SKEL_R_Thigh, SKEL_R_Calf);
				Bone(SKEL_L_Calf, SKEL_L_Foot); Bone(SKEL_R_Calf, SKEL_R_Foot);
			}

			// HeadCircle
			if (g_Options.Visuals.Players.EnableHeadBol)
			{
				Vector3D hp3 = g_Fivem.GetBonePosVec3(Current, SKEL_Head);
				if (hp3.x != 0.f || hp3.y != 0.f || hp3.z != 0.f)
				{
					ImVec2 hs = g_Fivem.WorldToScreen(hp3);
					if (hs.x != 0 && hs.y != 0)
					{
						float cv = Height / 28.f; if (cv < 4.f) cv = 4.f;
						ImColor col = FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HeadCircleColor);
						ImColor fillCol = ImColor(col.Value.x, col.Value.y, col.Value.z, col.Value.w * 0.35f);
						// sombra externa para contraste
						DrawList->AddCircleFilled(hs, cv + 1.5f, ImColor(0.f, 0.f, 0.f, col.Value.w * 0.5f), 32);
						// fill solido
						DrawList->AddCircleFilled(hs, cv, fillCol, 32);
						// borda solida
						DrawList->AddCircle(hs, cv, col, 32, 2.5f);
					}
				}
			}

			// Distance
			if (g_Options.Visuals.Players.EnableDistance)
			{
				try
				{
					// Calculate dynamic font size based on player height on screen
					float fontScale = Height / 100.f; // Scale based on player height
					fontScale = (fontScale < 0.6f) ? 0.6f : (fontScale > 1.2f ? 1.2f : fontScale); // Clamp between 0.6 and 1.2
					
					// Check if font is valid
					ImFont* font = FrameWork::Assets::InterMedium10;
					if (!font || !ImGui::GetIO().Fonts->IsBuilt())
						font = ImGui::GetIO().Fonts->Fonts[0];
					
					if (font)
					{
						std::string txt = std::to_string((int)Distance) + "m";
						
						ImGui::PushFont(font);
						ImVec2 ts = ImGui::CalcTextSize(txt.c_str());
						ts.x *= fontScale;
						ts.y *= fontScale;
						
						ImVec2 dp(PedLocation.x - ts.x / 2, PedCenterY + Height / 2 + Padding[2].y - 3);
						Padding[2].y += ts.y;
						
						// Draw with scale using DrawList directly (no window context needed)
						ImFont* scaledFont = font;
						float oldScale = scaledFont->Scale;
						scaledFont->Scale = fontScale;
						DrawList->AddText(scaledFont, scaledFont->FontSize * fontScale, dp + ImVec2(1, 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.DistanceColor[3]), txt.c_str());
						DrawList->AddText(scaledFont, scaledFont->FontSize * fontScale, dp, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.DistanceColor), txt.c_str());
						scaledFont->Scale = oldScale;
						
						ImGui::PopFont();
					}
				}
				catch (...)
				{
					// Silently catch any exception in distance rendering
				}
			}

			// HealthBar
			if (g_Options.Visuals.Players.HealthBar)
			{
				float Health = Current.StaticInfo.Ped->GetHealth(), MaxHealth = Current.StaticInfo.Ped->GetMaxHealth();
				if (g_Options.Visuals.Players.HealthBarType == 0)
				{
					ImVec2 dp = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2) - Padding[3] - ImVec2(4, 0);
					DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(3, Height + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.HealthBarColor[3]));
					DrawList->AddRectFilled(dp, dp + ImVec2(2, Height), FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HealthBarColor));
					Padding[3].x += 3;
				}
				if (g_Options.Visuals.Players.HealthBarType == 1)
				{
					ImVec2 dp = ImVec2(PedLocation.x + Width / 2, PedCenterY - Height / 2) + Padding[1] + ImVec2(2, 0);
					DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(3, Height + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.HealthBarColor[3]));
					DrawList->AddRectFilled(dp, dp + ImVec2(2, Height), FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HealthBarColor));
					Padding[1].x += 3;
				}
				if (g_Options.Visuals.Players.HealthBarType == 2)
				{
					ImVec2 dp = ImVec2(PedLocation.x - Width / 2, PedCenterY + Height / 2) + Padding[2];
					ImVec2 ds((Width / MaxHealth) * Health, 2);
					DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(Width + 1, 3), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.HealthBarColor[3]));
					DrawList->AddRectFilled(dp, dp + ds, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HealthBarColor));
					Padding[2].y += 3;
				}
				if (g_Options.Visuals.Players.HealthBarType == 3)
				{
					ImVec2 dp(PedLocation.x - Width / 2, PedCenterY - Height / 2 + Padding[2].y);
					ImVec2 ds((Width / MaxHealth) * Health, 2);
					DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(Width + 1, 3), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.HealthBarColor[3]));
					DrawList->AddRectFilled(dp, dp + ds, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.HealthBarColor));
					Padding[2].y += 5;
				}
			}

			// ArmorBar
			if (g_Options.Visuals.Players.AmorBar)
			{
				float Armor = Current.StaticInfo.Ped->GetArmor();
				if (g_Options.Visuals.Players.AmorBarType == 0)
				{
					ImVec2 dp = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2) - Padding[3] - ImVec2(8, 0);
					if (Armor > 0) { DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(3, Height + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.ArmorBarColor[3]));DrawList->AddRectFilled(dp, dp + ImVec2(2, Height), FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.ArmorBarColor)); }
					Padding[3].x += 3;
				}
				if (g_Options.Visuals.Players.AmorBarType == 1)
				{
					ImVec2 dp = ImVec2(PedLocation.x + Width / 2, PedCenterY - Height / 2) + Padding[1] + ImVec2(6, 0);
					if (Armor > 0) { DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(3, Height + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.ArmorBarColor[3]));DrawList->AddRectFilled(dp, dp + ImVec2(2, Height), FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.ArmorBarColor)); }
					Padding[1].x += 3;
				}
				if (g_Options.Visuals.Players.AmorBarType == 2)
				{
					ImVec2 dp = ImVec2(PedLocation.x - Width / 2, PedCenterY + Height / 2) + Padding[2];
					ImVec2 ds((Width / 100) * Armor, 2);
					if (Armor > 0) { DrawList->AddRectFilled(dp - ImVec2(1, 1), dp + ImVec2(Width + 1, 3), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.ArmorBarColor[3]));DrawList->AddRectFilled(dp, dp + ds, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.ArmorBarColor)); }
					Padding[2].y += 3;
				}
				if (g_Options.Visuals.Players.AmorBarType == 3)
				{
					ImVec2 dp = ImVec2(PedLocation.x - Width / 2, PedCenterY - Height / 2) - Padding[0];
					ImVec2 ds((Width / 100) * Armor, 2);
					if (Armor > 0) { DrawList->AddRectFilled(dp - ImVec2(1, 3), dp + ImVec2(Width + 1, 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.ArmorBarColor[3]));DrawList->AddRectFilled(dp, dp + ds, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.ArmorBarColor)); }
					Padding[0].y += 3;
				}
			}

			// Name
			if (g_Options.Visuals.Players.Name)
			{
				std::string PlayerName = Current.NetworkInfo.UserName;

				if (PlayerName.empty() || PlayerName == "Unknown")
				{
					if (!Current.StaticInfo.Name.empty() &&
						Current.StaticInfo.Name != "NPC" &&
						Current.StaticInfo.Name != "Unknown" &&
						Current.StaticInfo.Name != "** Invalid **")
					{
						PlayerName = Current.StaticInfo.Name;
					}
					else
					{
						PlayerName = Current.StaticInfo.bIsNPC ? "NPC" : "Unknown";
					}
				}

				if (PlayerName.length() > 32)
					PlayerName = PlayerName.substr(0, 32);

				ImFont* font = FrameWork::Assets::InterMedium10;
				if (!font || !ImGui::GetIO().Fonts->IsBuilt())
						font = ImGui::GetIO().Fonts->Fonts[0];

				if (font)
				{
					ImVec2 TextSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, PlayerName.c_str());
					ImVec2 TextPos = ImVec2(PedLocation.x - (TextSize.x / 2), PedCenterY - Height / 2.f - TextSize.y - Padding[0].y);
					Padding[0].y += TextSize.y + 1;

					DrawList->AddText(font, font->FontSize, ImVec2(TextPos.x + 1, TextPos.y + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.TextColor[3]), PlayerName.c_str());
					DrawList->AddText(font, font->FontSize, TextPos, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.TextColor), PlayerName.c_str());
				}
			}

			// WeaponName
			if (g_Options.Visuals.Players.WeaponName)
			{
				try
				{
					// Calculate dynamic font size based on player height on screen
					float fontScale = Height / 100.f; // Scale based on player height
					fontScale = (fontScale < 0.6f) ? 0.6f : (fontScale > 1.2f ? 1.2f : fontScale); // Clamp between 0.6 and 1.2
					
					// Check if font is valid
					ImFont* font = FrameWork::Assets::InterMedium10;
					if (!font || !ImGui::GetIO().Fonts->IsBuilt())
						font = ImGui::GetIO().Fonts->Fonts[0];
					
					if (font)
					{
						CWeaponManager* wm = Current.StaticInfo.Ped->GetWeaponManager();
						if (wm) {
							CWeaponInfo* wi = wm->GetWeaponInfo();
							if (wi) {
								std::string wn = wi->GetWeaponName();
								if (wn.size() > 0 && wn.size() < 64) {
									ImGui::PushFont(font);
									ImVec2 ts = ImGui::CalcTextSize(wn.c_str());
									ts.x *= fontScale;
									ts.y *= fontScale;
									
									ImVec2 dp(PedLocation.x - ts.x / 2, PedCenterY + Height / 2 + Padding[2].y - 3);
									Padding[2].y += ts.y;
									
									// Draw with scale using DrawList directly (no window context needed)
									ImFont* scaledFont = font;
									float oldScale = scaledFont->Scale;
									scaledFont->Scale = fontScale;
									DrawList->AddText(scaledFont, scaledFont->FontSize * fontScale, dp + ImVec2(1, 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Players.TextColor[3]), wn.c_str());
									DrawList->AddText(scaledFont, scaledFont->FontSize * fontScale, dp, FrameWork::Misc::Float4ToImColor(g_Options.Visuals.Players.TextColor), wn.c_str());
									scaledFont->Scale = oldScale;
									
									ImGui::PopFont();
								}
							}
						}
					}
				}
				catch (...)
				{
					// Silently catch any exception in weapon name rendering
				}
			}
			}
			catch (...)
			{
				// Silently catch any exception for this entity and continue
				continue;
			}
		}
		}
		catch (...)
		{
			// Silently catch any exception in the main loop
		}
	}
}
