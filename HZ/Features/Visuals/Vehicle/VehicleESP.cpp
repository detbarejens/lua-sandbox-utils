#pragma once
#include "VehicleESP.hpp"

#include "../../../FiveM-External.hpp"
#include "../../../Definations/Variables.hpp"
#include "../../../Utils/VisibilityCheck.hpp"

namespace Cheat
{
	void ESP::Vehicles()
	{
		if (!g_Fivem.GetLocalPlayerInfo().Ped)
			return;

		try
		{
			// Check if font is valid before starting
			ImFont* font = FrameWork::Assets::InterMedium10;
			if (!font || !ImGui::GetIO().Fonts->IsBuilt())
				font = ImGui::GetIO().Fonts->Fonts[0];
			
			if (!font)
				return;

			for (VehicleInfo Current : g_Fivem.GetVehicleList())
			{
				try
				{
					if (!Current.Vehicle)
						continue;
						
					ImVec2 Position = g_Fivem.WorldToScreen(Current.Vehicle->GetCoordinate());
					if (!g_Fivem.IsOnScreen(Position))
						continue;

					float OffsetY = 0;

					if (g_Options.Visuals.Vehicles.VisibleCheck)
					{
						bool isVisible = FrameWork::Visibility::IsVehicleVisible(Current.Vehicle);
						ImColor dotColor = isVisible ? ImColor(0, 255, 0, 255) : ImColor(255, 0, 0, 255);
						ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y - 6), 5, ImColor(0, 0, 0, 200));
						ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y - 6), 4, dotColor);
						OffsetY += 4;
					}

					if (g_Options.Visuals.Vehicles.Marker)
					{
						ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y + OffsetY), 4, ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.MarkerColor[3]));
						ImGui::GetBackgroundDrawList()->AddCircleFilled(ImVec2(Position.x, Position.y + OffsetY), 3, ImColor(g_Options.Visuals.Vehicles.MarkerColor[0], g_Options.Visuals.Vehicles.MarkerColor[1], g_Options.Visuals.Vehicles.MarkerColor[2], g_Options.Visuals.Vehicles.MarkerColor[3]));

						OffsetY += 12;
					}

					// Calculate dynamic font scale based on distance
					float vehicleDistance = Current.Vehicle->GetCoordinate().DistTo(g_Fivem.GetLocalPlayerInfo().WorldPos);
					float fontScale = 1.0f - (vehicleDistance / 500.0f); // Scale based on distance (max 500m)
					fontScale = (fontScale < 0.6f) ? 0.6f : (fontScale > 1.2f ? 1.2f : fontScale); // Clamp between 0.6 and 1.2

					ImGui::PushFont(font);

					if (g_Options.Visuals.Vehicles.Name)
					{
						std::string DisplayName = Current.Name;
						if (DisplayName.length() > 64)
							DisplayName = DisplayName.substr(0, 64);
						
						if (Current.Vehicle->GetLockState() == CARLOCK_LOCKED && g_Options.Visuals.Vehicles.ShowLockState)
						{
							DisplayName += XorStr(" - LOCKED");
						}
						else if (Current.Vehicle->GetLockState() == CARLOCK_UNLOCKED && g_Options.Visuals.Vehicles.ShowLockState)
						{
							DisplayName += XorStr(" - UNLOCKED");
						}
						
						ImVec2 TextSize = ImGui::CalcTextSize(DisplayName.c_str());
						TextSize.x *= fontScale;
						TextSize.y *= fontScale;

						// Draw with scale using DrawList directly (no window context needed)
						ImFont* scaledFont = font;
						float oldScale = scaledFont->Scale;
						scaledFont->Scale = fontScale;
						ImGui::GetBackgroundDrawList()->AddText(scaledFont, scaledFont->FontSize * fontScale, ImVec2(Position.x + 1 - TextSize.x / 2, Position.y + OffsetY + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.NameColor[3]), DisplayName.c_str());
						ImGui::GetBackgroundDrawList()->AddText(scaledFont, scaledFont->FontSize * fontScale, ImVec2(Position.x - TextSize.x / 2, Position.y + OffsetY), ImColor(g_Options.Visuals.Vehicles.NameColor[0], g_Options.Visuals.Vehicles.NameColor[1], g_Options.Visuals.Vehicles.NameColor[2], g_Options.Visuals.Vehicles.NameColor[3]), DisplayName.c_str());
						scaledFont->Scale = oldScale;

						OffsetY += TextSize.y + 2;
					}

					if (g_Options.Visuals.Vehicles.Distance)
					{
						char bfr[24];
						sprintf(bfr, ("%dm"), (int)vehicleDistance);

						ImVec2 TextSize = ImGui::CalcTextSize(bfr);
						TextSize.x *= fontScale;
						TextSize.y *= fontScale;

						// Draw with scale using DrawList directly (no window context needed)
						ImFont* scaledFont = font;
						float oldScale = scaledFont->Scale;
						scaledFont->Scale = fontScale;
						ImGui::GetBackgroundDrawList()->AddText(scaledFont, scaledFont->FontSize * fontScale, ImVec2(Position.x + 1 - TextSize.x / 2, Position.y + OffsetY + 1), ImColor(0.f, 0.f, 0.f, g_Options.Visuals.Vehicles.DistanceColor[3]), bfr);
						ImGui::GetBackgroundDrawList()->AddText(scaledFont, scaledFont->FontSize * fontScale, ImVec2(Position.x - TextSize.x / 2, Position.y + OffsetY), ImColor(g_Options.Visuals.Vehicles.DistanceColor[0], g_Options.Visuals.Vehicles.DistanceColor[1], g_Options.Visuals.Vehicles.DistanceColor[2], g_Options.Visuals.Vehicles.DistanceColor[3]), bfr);
						scaledFont->Scale = oldScale;
						
						OffsetY += TextSize.y + 2;
					}

					ImGui::PopFont();
				}
				catch (...)
				{
					// Silently catch any exception for this vehicle
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