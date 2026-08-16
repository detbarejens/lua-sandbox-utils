#include "Interface.hpp"
#include "../Definations/Cheat.hpp"
#include "../Features/LegitBot/AimBot.hpp"
#include "../Features/LegitBot/SilentAim.hpp"
#include "../Features/LegitBot/TriggerBot.hpp"
#include "../Features/Misc/Exploits.hpp"
#include "Assets/Assets.hpp"
#include "Assets/Data/FontAwesome.hpp"
#include "DiscordAvatar.hpp"
#include "Overlay.hpp"
#include "../Utils/CloudConfig.hpp"
#include "../Utils/LocalConfig.hpp"
#include "../Utils/Memory.hpp"
#include "../Definations/Brand.hpp"
#include "../Definations/TrinityLock.hpp"
#include "../FrameWork/Utilities/Discord.hpp"
#if defined(TRINITY_DEV) && TRINITY_DEV
#include "../Features/Dev/DevPanel.hpp"
#endif
#include "../Security/XorStr.hpp"
#include "../Slate/include/slate.h"
#include <thread>
#include <map>
#include <cctype>

static bool bAvatarLoaded = false;
static bool bAvatarLoading = false;

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
static void SyncEspPreviewBarPositions()
{
	static e_esp_item_pos lastHealthPos = esppos_left;
	static e_esp_item_pos lastArmorPos = esppos_right;

	const e_esp_item_pos currentHealthPos = (e_esp_item_pos)g_Options.Visuals.Players.HealthBarType;
	const e_esp_item_pos currentArmorPos = (e_esp_item_pos)g_Options.Visuals.Players.AmorBarType;

	if (lastHealthPos == currentHealthPos && lastArmorPos == currentArmorPos)
		return;

	for (int pos = 0; pos < 4; ++pos)
	{
		auto items_in_pos = g_esppreview->get_items_by_pos((e_esp_item_pos)pos);
		for (auto* item : items_in_pos)
		{
			if (item->name == "Health")
			{
				item->pos = currentHealthPos;
				item->posvec2 = ImVec2{ 0, 0 };
			}
			else if (item->name == "Armor")
			{
				item->pos = currentArmorPos;
				item->posvec2 = ImVec2{ 0, 0 };
			}
		}
	}

	lastHealthPos = currentHealthPos;
	lastArmorPos = currentArmorPos;
}

static void DrawTrinityEspPreviewPanel()
{
	using namespace ImGui;

	SyncEspPreviewBarPositions();

	const ImVec2 region = GetContentRegionAvail();
	const float previewH = ImMax(380.f, region.y);
	const ImVec2 canvasMin = GetCursorScreenPos();
	const ImVec2 canvasMax = canvasMin + ImVec2{ region.x, previewH };

	ImDrawList* dl = GetWindowDrawList();
	dl->AddRect(canvasMin, canvasMax, IM_COL32(99, 102, 241, 40), 12.f);

	PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.f, 0.f });
	BeginChild("##trinity_esp_canvas", ImVec2{ region.x, previewH }, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
	{
		const ImVec2 cp = GetWindowPos();
		const ImVec2 cs = GetWindowSize();
		const ImVec2 center = cp + ImVec2{ cs.x * 0.5f, cs.y * 0.5f - 6.f };

		g_esppreview->draw(
			center,
			&g_Options.Visuals.Players.EnableBox,
			g_Options.Visuals.Players.BoxColor,
			&g_Options.Visuals.Players.BoxType,
			&g_Options.Visuals.Players.Skeleton,
			g_Options.Visuals.Players.SkeletonColor,
			nullptr,
			nullptr
		);

		for (int pos = 0; pos < 4; ++pos)
		{
			auto items_in_pos = g_esppreview->get_items_by_pos((e_esp_item_pos)pos);
			for (auto* item : items_in_pos)
			{
				if (item->name == "Health")
					g_Options.Visuals.Players.HealthBarType = (int)item->pos;
				else if (item->name == "Armor")
					g_Options.Visuals.Players.AmorBarType = (int)item->pos;
			}
		}
	}
	EndChild();
	PopStyleVar();

	const char* pedLabel = "s_f_y_hooker_03";
	const ImVec2 labelSize = CalcTextSize(pedLabel);
	dl->AddText(
		ImVec2((canvasMin.x + canvasMax.x - labelSize.x) * 0.5f, canvasMax.y - 20.f),
		IM_COL32(100, 108, 120, 200),
		pedLabel
	);

	Dummy(ImVec2{ region.x, previewH });
}

static std::string g_ActiveConfigName = "none";

static float TrinityStatusWidth(const char* label, const char* value)
{
	return ImGui::CalcTextSize(label).x + 6.f + ImGui::CalcTextSize(value).x;
}

static void TrinityStatusItem(ImDrawList* dl, ImVec2 pos, const char* label, const char* value, ImU32 valueCol = IM_COL32(180, 184, 196, 255))
{
	dl->AddText(pos, IM_COL32(107, 114, 128, 255), label);
	dl->AddText(ImVec2{ pos.x + ImGui::CalcTextSize(label).x + 6.f, pos.y }, valueCol, value);
}

static void TrinitySpacedText(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 col, float extra)
{
	ImVec2 p = pos;
	for (const char* c = text; *c; ++c)
	{
		char buf[2] = { *c, 0 };
		dl->AddText(p, col, buf);
		p.x += ImGui::CalcTextSize(buf).x + extra;
	}
}

static ImVec2 TrinityKeyChipSize(const char* label)
{
	const ImVec2 ts = ImGui::CalcTextSize(label);
	return ImVec2{ ImMax(52.f, ts.x + 16.f), ts.y + 10.f };
}

static void TrinityKeyChip(ImDrawList* dl, ImVec2 pos, const char* label)
{
	const ImVec2 size = TrinityKeyChipSize(label);
	dl->AddRectFilled(pos, pos + size, IM_COL32(38, 38, 42, 255), 6.f);
	dl->AddRect(pos, pos + size, IM_COL32(255, 255, 255, 16), 6.f);
	const ImVec2 ts = ImGui::CalcTextSize(label);
	dl->AddText(pos + ImVec2{ (size.x - ts.x) * 0.5f, (size.y - ts.y) * 0.5f }, IM_COL32(156, 163, 175, 255), label);
}

static float HzUserChipWidth(const char* name, const char* role)
{
	return 44.f + ImMax(ImGui::CalcTextSize(name).x, ImGui::CalcTextSize(role).x);
}

static void HzUserChip(ImDrawList* dl, ImVec2 pos, const char* name, const char* role, ID3D11ShaderResourceView* avatar)
{
	const ImU32 accent = IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255);
	const ImVec2 center = pos + ImVec2{ 16.f, 16.f };
	(void)avatar;
	dl->AddCircleFilled(center, 16.f, IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 48));
	dl->AddCircle(center, 16.f, accent, 0, 1.4f);
	char initial[2] = { name && name[0] ? (char)toupper((unsigned char)name[0]) : 'H', 0 };
	const ImVec2 isz = ImGui::CalcTextSize(initial);
	dl->AddText(ImVec2{ center.x - isz.x * 0.5f, center.y - isz.y * 0.5f }, accent, initial);

	dl->AddText(pos + ImVec2{ 40.f, 2.f }, IM_COL32(245, 245, 247, 255), name);
	dl->AddText(pos + ImVec2{ 40.f, 18.f }, IM_COL32(140, 146, 158, 255), role);
	dl->AddCircleFilled(pos + ImVec2{ 40.f + ImGui::CalcTextSize(role).x + 10.f, 25.f }, 3.4f, accent);
	dl->AddCircleFilled(pos + ImVec2{ 40.f + ImGui::CalcTextSize(role).x + 10.f, 25.f }, 6.f, IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 50));
}
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#include <filesystem>

namespace fs = std::filesystem;


namespace FrameWork
{
	void Interface::Initialize(HWND Window, HWND TargetWindow, ID3D11Device* Device, ID3D11DeviceContext* DeviceContext)
	{
		hWindow = Window;
		hTargetWindow = TargetWindow;
		IDevice = Device;

		ImGui::CreateContext();
		ImGui_ImplWin32_Init(hWindow);
		ImGui_ImplDX11_Init(Device, DeviceContext);

		ImGui::GetIO().IniFilename = nullptr;


		g_style->setup();
		g_lang->initialize();
		slate->initialize_fonts();
		FrameWork::Assets::Initialize(Device);
		ImGui_ImplDX11_CreateDeviceObjects();

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		widgets->nav.tabs = {
			{ aiming_2_filled,     BRAND_NAV_AIM,     { { settings_3_filled, "Aimbot" }, { sword_filled, "Silent" }, { more_3_filled, "Trigger" }, { more_3_filled, "Crosshair" } }, 0, 0, {}, BRAND_NAV_AIM_SUB },
			{ eye_2_filled,        BRAND_NAV_VISUALS, { { settings_3_filled, "Visible" }, { more_3_filled, "Invisible" }, { settings_3_filled, "Self" }, { more_3_filled, "NPC" }, { car_2_filled, "Vehicle" }, { more_3_filled, "Radar" }, { more_3_filled, "Props" }, { settings_3_filled, "Alerts" } }, 0, 0, {}, BRAND_NAV_VIS_SUB },
			{ user_1_filled,       BRAND_NAV_LISTS,   { { settings_3_filled, "Players" }, { car_2_filled, "Vehicles" } }, 0, 0, {}, BRAND_NAV_LIST_SUB },
			{ layers_filled,       BRAND_NAV_EXPLOITS,{ { settings_3_filled, "Self" }, { more_3_filled, "Weapon" }, { car_2_filled, "Vehicle" }, { more_3_filled, "Misc" } }, 0, 0, {}, BRAND_NAV_FEAT_SUB },
			{ settings_3_filled,   BRAND_NAV_CONFIGS, { { settings_3_filled, "Settings" }, { folder_filled, "Configs" } }, 0, 0, {}, BRAND_NAV_SET_SUB },
#if defined(TRINITY_DEV) && TRINITY_DEV
			{ code_filled,         "Developer",       { { code_filled, "Code" }, { file_code_filled, "Load File" }, { settings_3_filled, "Lua State" }, { settings_3_filled, "Settings" }, { search_filled, "Find Triggers" }, { arrow_right_filled, "Client > Server" }, { arrow_left_filled, "Server > Client" }, { server_filled, "Resources" }, { windows_filled, "Nui" } }, 0, 0, {}, "Dev tools" },
#endif
		};
#else
		widgets->nav.tabs = {
			{ aiming_2_filled,  BRAND_NAV_AIM,     { { settings_3_filled, "Aimbot" }, { sword_filled, "Silent" }, { more_3_filled, "Trigger" }, { more_3_filled, "Crosshair" } } },
			{ eye_2_filled,     BRAND_NAV_VISUALS, { { settings_3_filled, "Player" }, { car_2_filled, "Vehicle" }, { more_3_filled, "Color"   }, { more_3_filled, "News"      } } },
			{ more_3_filled,    BRAND_NAV_EXPLOITS,{ { settings_3_filled, "Self" }, { more_3_filled, "Weapon" }, { more_3_filled, "Others" } } },
			{ folder_filled,    BRAND_NAV_CONFIGS, { } },
		};
#endif


		widgets->nav.addpage(0, [&] {

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
			ImGui::BeginGroup();
			{
				widgets->child.begin("##aim_left", 1);
				{
					widgets->trinity_section_header("Aimbot", &g_Options.LegitBot.AimBot.Enabled);
					widgets->trinity_setting_row("Keybind", [&] {
						widgets->binder("##aim_key", &g_Options.LegitBot.AimBot.KeyBind);
					});
					widgets->checkbox("Visible Check", &g_Options.LegitBot.AimBot.VisibleCheck);
					widgets->checkbox("Target NPC", &g_Options.LegitBot.AimBot.TargetNPC);
					widgets->checkbox("Show FOV", &g_Options.LegitBot.AimBot.ShowFov);
					widgets->coloredit("FOV Color", g_Options.LegitBot.AimBot.FovColor);
					widgets->checkbox("Prediction", &g_Options.LegitBot.AimBot.Prediction);
					widgets->checkbox("Aim Assist", &g_Options.LegitBot.AimBot.AimAssist);
					widgets->checkbox("Sticky Aim", &g_Options.LegitBot.AimBot.StickyAim);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("##aim_right", 2);
				{
					widgets->trinity_section("Configuration");
					widgets->combo("Hitbox", &g_Options.LegitBot.AimBot.HitBox, { "Head", "Neck", "Chest" });
					widgets->combo("Priority By", &g_Options.LegitBot.AimBot.ClosestFov, { "FOV", "Distance", "Health" });
					widgets->sliderint("FOV", &g_Options.LegitBot.AimBot.FOV, 0, 300);
					widgets->sliderint("Max Distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 600);
					widgets->sliderint("Smooth X", &g_Options.LegitBot.AimBot.SmoothHorizontal, 0, 100);
					widgets->sliderint("Smooth Y", &g_Options.LegitBot.AimBot.SmoothVertical, 0, 100);
					if (g_Options.LegitBot.AimBot.Prediction)
						widgets->sliderint("Prediction Mult", &g_Options.LegitBot.AimBot.PredictionMultiplier, 0, 200);
					if (g_Options.LegitBot.AimBot.AimAssist)
						widgets->sliderint("Assist Strength", &g_Options.LegitBot.AimBot.AimAssistStrength, 0, 100);
					if (g_Options.LegitBot.AimBot.StickyAim)
						widgets->sliderint("Sticky Strength", &g_Options.LegitBot.AimBot.StickyAimStrength, 0, 100);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
#else
			ImGui::BeginGroup();
			{
				widgets->child.begin("Aimbot", 1);
				{
					widgets->checkbox("Enabled", &g_Options.LegitBot.AimBot.Enabled);
					widgets->binder("Keybind##aim", &g_Options.LegitBot.AimBot.KeyBind);
					widgets->checkbox("Aim Lock", &g_Options.LegitBot.AimBot.StrictLock);
					widgets->checkbox("Visible Check", &g_Options.LegitBot.AimBot.VisibleCheck);
					widgets->checkbox("Target NPC", &g_Options.LegitBot.AimBot.TargetNPC);
					widgets->checkbox("Show Fov", &g_Options.LegitBot.AimBot.ShowFov);
					widgets->coloredit("FOV Color", g_Options.LegitBot.AimBot.FovColor);
					widgets->combo("Hitbox", &g_Options.LegitBot.AimBot.HitBox, { "Head", "Neck", "Chest" });
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##aim", 2);
				{
					widgets->sliderint("FOV", &g_Options.LegitBot.AimBot.FOV, 0, 300);
					widgets->sliderint("Max Distance", &g_Options.LegitBot.AimBot.MaxDistance, 0, 600);
					widgets->sliderint("Smooth X", &g_Options.LegitBot.AimBot.SmoothHorizontal, 0, 100);
					widgets->sliderint("Smooth Y", &g_Options.LegitBot.AimBot.SmoothVertical, 0, 100);
					widgets->combo("Priority By", &g_Options.LegitBot.AimBot.ClosestFov, { "FOV", "Distance", "Health" });
					
					widgets->checkbox("Prediction", &g_Options.LegitBot.AimBot.Prediction);
					widgets->sliderint("Prediction Mult", &g_Options.LegitBot.AimBot.PredictionMultiplier, 0, 200);
					widgets->checkbox("Aim Assist", &g_Options.LegitBot.AimBot.AimAssist);
					widgets->sliderint("Assist Strength", &g_Options.LegitBot.AimBot.AimAssistStrength, 0, 100);
					widgets->checkbox("Sticky Aim", &g_Options.LegitBot.AimBot.StickyAim);
					widgets->sliderint("Sticky Strength", &g_Options.LegitBot.AimBot.StickyAimStrength, 0, 100);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
#endif
			});

		// Subtab 1 – Silent
		widgets->nav.addpage(0, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Silent", 1);
				{
					widgets->checkbox("Enabled", &g_Options.LegitBot.SilentAim.Enabled);
					widgets->binder("Keybind##silent", &g_Options.LegitBot.SilentAim.KeyBind);
					widgets->checkbox("Visible Check", &g_Options.LegitBot.SilentAim.VisibleCheck);
					widgets->checkbox("Target NPC", &g_Options.LegitBot.SilentAim.ShotNPC);
					widgets->checkbox("Show Fov", &g_Options.LegitBot.SilentAim.ShowFOV);
					widgets->coloredit("FOV Color##silent", g_Options.LegitBot.SilentAim.FovColor);
					widgets->combo("HitBox", &g_Options.LegitBot.SilentAim.HitBox, { "Head", "Neck", "Chest" });
				}
				widgets->child.end();
				
				widgets->child.begin("Magic Bullets", 8);
				{
					widgets->checkbox("Enabled##magic", &g_Options.LegitBot.MagicBullets.Enabled);
					widgets->binder("Keybind##magic", &g_Options.LegitBot.MagicBullets.KeyBind);
					widgets->checkbox("Target NPC##magic", &g_Options.LegitBot.MagicBullets.TargetNPC);
					widgets->checkbox("Show Fov##magic", &g_Options.LegitBot.MagicBullets.ShowFOV);
					widgets->sliderint("FOV##magic", &g_Options.LegitBot.MagicBullets.FOV, 0, 300);
					widgets->coloredit("FOV Color##magic", g_Options.LegitBot.MagicBullets.FovColor);

				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##silent", 2);
				{
					widgets->sliderint("FOV", &g_Options.LegitBot.SilentAim.Fov, 0, 300);
					widgets->sliderint("Max Distance", &g_Options.LegitBot.SilentAim.MaxDistance, 0, 600);
					widgets->sliderint("Miss Chance", &g_Options.LegitBot.SilentAim.MissChance, 0, 50);
					widgets->combo("Priority By", &g_Options.LegitBot.SilentAim.ClosestFov, { "FOV", "Distance", "Health" });
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		
		widgets->nav.addpage(0, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Trigger", 1);
				{
					widgets->checkbox("Enabled", &g_Options.LegitBot.Trigger.Enabled);
					widgets->binder("Keybind##trig", &g_Options.LegitBot.Trigger.KeyBind);
					widgets->checkbox("Allow NPC", &g_Options.LegitBot.Trigger.ShotNPC);
					widgets->checkbox("Visible Check", &g_Options.LegitBot.Trigger.VisibleCheck);
					widgets->checkbox("Show Fov##trig", &g_Options.LegitBot.Trigger.ShowFOV);
					widgets->coloredit("FOV Color##trig", g_Options.LegitBot.Trigger.FovColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##trig", 2);
				{
					widgets->sliderint("Max Distance", &g_Options.LegitBot.Trigger.MaxDistance, 0, 600);
					widgets->sliderint("FOV##trig", &g_Options.LegitBot.Trigger.FOV, 0, 300);
					widgets->sliderint("Reaction Time", &g_Options.LegitBot.Trigger.ReactionTime, 0, 50);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});


		widgets->nav.addpage(0, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Crosshair", 1);
				{
					widgets->checkbox("Enabled", &g_Options.Crosshair.Enabled);
					widgets->checkbox("Show Dot", &g_Options.Crosshair.ShowDot);
					widgets->checkbox("Show Lines", &g_Options.Crosshair.ShowLines);
					widgets->checkbox("Show Outline", &g_Options.Crosshair.ShowOutline);
					widgets->checkbox("Dynamic Gap", &g_Options.Crosshair.DynamicGap);
					widgets->combo("Style", &g_Options.Crosshair.Style, { "Cross", "Circle", "Cross+Circle", "Dot Only" });
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##cross", 2);
				{
					widgets->sliderint("Size", &g_Options.Crosshair.Size, 1, 30);
					widgets->sliderint("Gap", &g_Options.Crosshair.Gap, 0, 20);
					widgets->sliderint("Thickness", &g_Options.Crosshair.Thickness, 1, 8);
					widgets->sliderint("Dot Size", &g_Options.Crosshair.DotSize, 1, 8);
					widgets->sliderint("Outline Size", &g_Options.Crosshair.OutlineThickness, 1, 4);
					widgets->sliderint("Rounding", &g_Options.Crosshair.Rounding, 0, 12);
					widgets->coloredit("Color", g_Options.Crosshair.Color);
					widgets->coloredit("Dot Color", g_Options.Crosshair.DotColor);
					widgets->coloredit("Outline Color", g_Options.Crosshair.OutlineColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// --- Tab 1: Visuals ---
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		// Subtab 0 – Visible
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("##esp_settings", 1);
				{
					widgets->trinity_section_header("ESP Settings", &g_Options.Visuals.Players.Enabled);

					const float halfW = (ImGui::GetContentRegionAvail().x - 8.f) * 0.5f;
					auto gridRow = [&](const char* left, bool* leftVal, const char* right, bool* rightVal) {
						widgets->trinity_toggle_row(left, leftVal, halfW);
						ImGui::SameLine(0.f, 8.f);
						widgets->trinity_toggle_row(right, rightVal, halfW);
					};

					gridRow("Box ESP", &g_Options.Visuals.Players.EnableBox, "Skeleton", &g_Options.Visuals.Players.Skeleton);
					gridRow("Name", &g_Options.Visuals.Players.Name, "Distance", &g_Options.Visuals.Players.EnableDistance);
					gridRow("Health Bar", &g_Options.Visuals.Players.HealthBar, "Armor Bar", &g_Options.Visuals.Players.AmorBar);
					gridRow("Weapon", &g_Options.Visuals.Players.WeaponName, "Head Circle", &g_Options.Visuals.Players.EnableHeadBol);

					widgets->trinity_divider();
					widgets->trinity_section("Filters");
					widgets->trinity_setting_row("Keybind", [&] {
						widgets->binder("##esp_key", &g_Options.Visuals.Players.ToggleKey);
					});
					widgets->checkbox("Bind ESP", &g_Options.Visuals.Players.Toogle);
					widgets->checkbox("Visible Check", &g_Options.Visuals.Players.VisibleCheck);

					widgets->trinity_divider();
					widgets->trinity_section("Distance");
					widgets->sliderint("Render Distance", &g_Options.Visuals.Players.RenderDistance, 0, 500);

					if (g_Options.Visuals.Players.EnableBox)
					{
						widgets->trinity_divider();
						widgets->trinity_section("Box Style");
						widgets->combo("Box Type", &g_Options.Visuals.Players.BoxType, { "Normal", "Corner" });
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("##esp_preview", 2);
				{
					widgets->trinity_section("ESP Preview");
					DrawTrinityEspPreviewPanel();
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 1 – Invisible
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Invisible Colors", 1);
				{
					widgets->coloredit("Invisible Box Color", g_Options.Visuals.Players.InvisibleVisibleBoxColor);
					widgets->coloredit("Invisible Skel Color", g_Options.Visuals.Players.InvisibleSkeletonColor);
					widgets->coloredit("Visible Box Color", g_Options.Visuals.Players.BoxColor);
					widgets->coloredit("Visible Skel Color", g_Options.Visuals.Players.SkeletonColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Visibility", 2);
				{
					widgets->checkbox("Visible Check", &g_Options.Visuals.Players.VisibleCheck);
					widgets->coloredit("HealthBar Color", g_Options.Visuals.Players.HealthBarColor);
					widgets->coloredit("HeadCircle Color", g_Options.Visuals.Players.HeadCircleColor);
					widgets->coloredit("Aimbot FOV Color", g_Options.LegitBot.AimBot.FovColor);
					widgets->coloredit("Silent FOV Color", g_Options.LegitBot.SilentAim.FovColor);
					widgets->coloredit("Trigger FOV Color", g_Options.LegitBot.Trigger.FovColor);
					widgets->coloredit("Magic FOV Color", g_Options.LegitBot.MagicBullets.FovColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 2 – Self
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Self ESP", 1);
				{
					widgets->checkbox("Show LocalPlayer", &g_Options.Visuals.Players.ShowLocalPlayer);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 3 – NPC
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("NPC Filters", 1);
				{
					widgets->checkbox("Show NPC", &g_Options.Visuals.Players.ShowNPC);
					widgets->checkbox("Hide Dead Players", &g_Options.Visuals.Players.ExcludeDeads);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 4 – Vehicle
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle", 1);
				{
					widgets->checkbox("Enabled", &g_Options.Visuals.Vehicles.Enabled);
					widgets->binder("Keybind##vehesp", &g_Options.Visuals.Vehicles.ToggleKey);
					widgets->checkbox("Bind ESP", &g_Options.Visuals.Vehicles.Toogle);
					widgets->checkbox("Show Lock State", &g_Options.Visuals.Vehicles.ShowLockState);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle Elements", 2);
				{
					widgets->checkbox("Enable Name", &g_Options.Visuals.Vehicles.Name);
					widgets->checkbox("Enable Distance", &g_Options.Visuals.Vehicles.Distance);
					widgets->checkbox("Enable Marker", &g_Options.Visuals.Vehicles.Marker);
					widgets->checkbox("Visible Check", &g_Options.Visuals.Vehicles.VisibleCheck);
					widgets->coloredit("Marker Color", g_Options.Visuals.Vehicles.MarkerColor);
					widgets->coloredit("Distance Color", g_Options.Visuals.Vehicles.DistanceColor);
					widgets->coloredit("Text Color", g_Options.Visuals.Vehicles.NameColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 5 – Radar
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Radar", 1);
				{
					widgets->checkbox("Radar", &g_Options.Visuals.Players.RadarEnabled);
					widgets->sliderint("Render Distance", &g_Options.Visuals.Players.RenderDistance, 0, 500);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 6 – Props
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Extras", 1);
				{
					widgets->checkbox("Line To Head", &g_Options.Visuals.Players.LineToHead);
					widgets->checkbox("Crosshair On Target", &g_Options.Visuals.Players.CrosshairOnTarget);
					widgets->checkbox("Pulse Circle", &g_Options.Visuals.Players.PulseCircle);
					widgets->checkbox("Rainbow Skeleton", &g_Options.Visuals.Players.RainbowSkeleton);
					widgets->checkbox("Spinning Ring", &g_Options.Visuals.Players.SpinningRing);
					widgets->checkbox("Box Breath", &g_Options.Visuals.Players.BoxBreath);
					widgets->checkbox("Health Bar 3D", &g_Options.Visuals.Players.HealthBar3D);
					widgets->checkbox("Enemy Arrow", &g_Options.Visuals.Players.EnemyArrow);
					widgets->checkbox("Distance Rings", &g_Options.Visuals.Players.DistanceRings);
					widgets->checkbox("Scan Line", &g_Options.Visuals.Players.ScanLine);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Extra Colors", 2);
				{
					widgets->coloredit("Snap Line Color", g_Options.Visuals.Players.SnapLineColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 7 – Alerts
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("##observer_alert", 1);
				{
					widgets->trinity_section_header("Observer", &g_Options.Visuals.Players.ObserverAlert);
					widgets->checkbox("Show On-Screen HUD", &g_Options.Visuals.Players.ObserverAlertShowHud);
					if (widgets->button("Reset HUD Position", ImVec2(0.f, 28.f)))
					{
						g_Options.Visuals.Players.ObserverAlertHudX = 0.5f;
						g_Options.Visuals.Players.ObserverAlertHudY = 0.56f;
					}
					widgets->sliderint("Look Threshold %", &g_Options.Visuals.Players.ObserverLookThreshold, 70, 99);
					widgets->sliderint("Observer Range", &g_Options.Visuals.Players.ObserverMaxDistance, 50, 500);
					widgets->sliderint("Awareness FOV", &g_Options.Visuals.Players.ObserverCanSeeFov, 20, 90);
					widgets->checkbox("Only Visible Players", &g_Options.Visuals.Players.ObserverVisibleCheck);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("##aiming_alert", 2);
				{
					widgets->trinity_section_header("Aiming Alert", &g_Options.Visuals.Players.AimingAlert);
					widgets->checkbox("Show Aiming HUD", &g_Options.Visuals.Players.AimingAlertShowHud);
					if (widgets->button("Reset Aiming HUD", ImVec2(0.f, 28.f)))
					{
						g_Options.Visuals.Players.AimingAlertHudX = 0.5f;
						g_Options.Visuals.Players.AimingAlertHudY = 0.64f;
					}
					widgets->sliderint("Aim Threshold %", &g_Options.Visuals.Players.AimingLookThreshold, 70, 99);
					widgets->sliderint("Aiming Range", &g_Options.Visuals.Players.AimingMaxDistance, 30, 400);
					widgets->sliderint("Aiming FOV", &g_Options.Visuals.Players.AimingAwarenessFov, 10, 60);
					widgets->checkbox("Only Visible Aimers", &g_Options.Visuals.Players.AimingVisibleCheck);

					widgets->trinity_divider();
					const bool wasProjectMonitor = g_Options.General.SecondMonitor;
					widgets->trinity_section_header(BRAND_PROJECT_MONITOR, &g_Options.General.SecondMonitor);
					if (g_Options.General.SecondMonitor != wasProjectMonitor)
					{
						if (g_Options.General.SecondMonitor)
						{
							if (g_Options.General.MonitorIndex < 0)
								g_Options.General.MonitorIndex = 0;
							FrameWork::Overlay::SetMonitorIndex(g_Options.General.MonitorIndex);
						}
						else
						{
							g_Options.General.MonitorIndex = -1;
						}
						RefreshWindowStyle();
					}

					if (g_Options.General.SecondMonitor)
					{
						static std::vector<std::string> monitorLabels;
						static std::vector<std::string_view> monitorItems;
						static int monitorSelection = 0;
						static bool monitorsInit = false;

						if (!monitorsInit)
						{
							monitorLabels = FrameWork::Overlay::GetMonitorNames();
							if (monitorLabels.empty())
								monitorLabels.push_back("Primary Monitor");

							monitorItems.clear();
							for (const auto& name : monitorLabels)
								monitorItems.push_back(name);

							monitorSelection = g_Options.General.MonitorIndex < 0 ? 0 : g_Options.General.MonitorIndex;
							if (monitorSelection >= (int)monitorItems.size())
								monitorSelection = 0;

							monitorsInit = true;
						}

						if (widgets->combo("Output display", &monitorSelection, monitorItems))
						{
							g_Options.General.MonitorIndex = monitorSelection;
							FrameWork::Overlay::SetMonitorIndex(monitorSelection);
							RefreshWindowStyle();
						}
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});
#else
		// Subtab 0 – Player
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Player", 1);
				{
					widgets->checkbox("Enabled", &g_Options.Visuals.Players.Enabled);
					widgets->binder("Keybind##esp", &g_Options.Visuals.Players.ToggleKey);
					widgets->checkbox("Bind ESP", &g_Options.Visuals.Players.Toogle);
					widgets->checkbox("Show NPC", &g_Options.Visuals.Players.ShowNPC);
					widgets->checkbox("Hide Dead Players", &g_Options.Visuals.Players.ExcludeDeads);
					widgets->checkbox("Show LocalPlayer", &g_Options.Visuals.Players.ShowLocalPlayer);
					widgets->checkbox("Visible Check", &g_Options.Visuals.Players.VisibleCheck);
					widgets->sliderint("Render Distance", &g_Options.Visuals.Players.RenderDistance, 0, 500);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##player", 2);
				{
					widgets->checkbox("Enable Box", &g_Options.Visuals.Players.EnableBox);
					if (g_Options.Visuals.Players.EnableBox)
						widgets->combo("Box", &g_Options.Visuals.Players.BoxType, { "Normal", "Corner" });
					widgets->checkbox("Enable Distance", &g_Options.Visuals.Players.EnableDistance);
					widgets->checkbox("Head Circle", &g_Options.Visuals.Players.EnableHeadBol);
					widgets->checkbox("Radar", &g_Options.Visuals.Players.RadarEnabled);
					widgets->checkbox("Enable Skeleton", &g_Options.Visuals.Players.Skeleton);
					widgets->checkbox("Enable Name", &g_Options.Visuals.Players.Name);
					widgets->checkbox("Enable HealthBar", &g_Options.Visuals.Players.HealthBar);
					if (g_Options.Visuals.Players.HealthBar)
						widgets->combo("HealthBar", &g_Options.Visuals.Players.HealthBarType, { "Top", "Right", "Bottom", "Left" });
					widgets->checkbox("Enable ArmorBar", &g_Options.Visuals.Players.AmorBar);
					if (g_Options.Visuals.Players.AmorBar)
						widgets->combo("ArmorBar", &g_Options.Visuals.Players.AmorBarType, { "Top", "Right", "Bottom", "Left" });
					widgets->checkbox("Enable WeaponName", &g_Options.Visuals.Players.WeaponName);
					widgets->checkbox("Chams", &g_Options.Visuals.Players.Chams);
					if (g_Options.Visuals.Players.Chams)
						widgets->combo("Chams Style", &g_Options.Visuals.Players.ChamsStyle, { "Solid", "Wireframe", "Glow" });
					widgets->checkbox("Damage Numbers", &g_Options.Visuals.Players.DamageNumbers);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 1 – Vehicle
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle", 1);
				{
					widgets->checkbox("Enabled", &g_Options.Visuals.Vehicles.Enabled);
					widgets->binder("Keybind##vehesp", &g_Options.Visuals.Vehicles.ToggleKey);
					widgets->checkbox("Bind ESP", &g_Options.Visuals.Vehicles.Toogle);
					widgets->checkbox("Show Lock State", &g_Options.Visuals.Vehicles.ShowLockState);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Settings##veh", 2);
				{
					widgets->checkbox("Enable Name", &g_Options.Visuals.Vehicles.Name);
					widgets->checkbox("Enable Distance", &g_Options.Visuals.Vehicles.Distance);
					widgets->checkbox("Enable Marker", &g_Options.Visuals.Vehicles.Marker);
					widgets->checkbox("Visible Check", &g_Options.Visuals.Vehicles.VisibleCheck);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 2 – Color
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Player Color", 1);
				{
					widgets->coloredit("Visible Box Color", g_Options.Visuals.Players.BoxColor);
					widgets->coloredit("Invisible Box Color", g_Options.Visuals.Players.InvisibleVisibleBoxColor);
					widgets->coloredit("Visible Skel Color", g_Options.Visuals.Players.SkeletonColor);
					widgets->coloredit("Invisible Skel Color", g_Options.Visuals.Players.InvisibleSkeletonColor);
					widgets->coloredit("HealthBar Color", g_Options.Visuals.Players.HealthBarColor);
					widgets->coloredit("HeadCircle Color", g_Options.Visuals.Players.HeadCircleColor);
					widgets->coloredit("Aimbot FOV Color", g_Options.LegitBot.AimBot.FovColor);
					widgets->coloredit("Silent FOV Color", g_Options.LegitBot.SilentAim.FovColor);
					widgets->coloredit("Trigger FOV Color", g_Options.LegitBot.Trigger.FovColor);
					widgets->coloredit("Magic FOV Color", g_Options.LegitBot.MagicBullets.FovColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle Color", 2);
				{
					widgets->coloredit("Marker Color", g_Options.Visuals.Vehicles.MarkerColor);
					widgets->coloredit("Distance Color", g_Options.Visuals.Vehicles.DistanceColor);
					widgets->coloredit("Text Color", g_Options.Visuals.Vehicles.NameColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		// Subtab 3 – News/Extras
		widgets->nav.addpage(1, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Extras", 1);
				{
					widgets->checkbox("Line To Head", &g_Options.Visuals.Players.LineToHead);
					widgets->checkbox("Crosshair On Target", &g_Options.Visuals.Players.CrosshairOnTarget);
					widgets->checkbox("Pulse Circle", &g_Options.Visuals.Players.PulseCircle);
					widgets->checkbox("Rainbow Skeleton", &g_Options.Visuals.Players.RainbowSkeleton);
					widgets->checkbox("Spinning Ring", &g_Options.Visuals.Players.SpinningRing);
					widgets->checkbox("Box Breath", &g_Options.Visuals.Players.BoxBreath);
					widgets->checkbox("Health Bar 3D", &g_Options.Visuals.Players.HealthBar3D);
					widgets->checkbox("Enemy Arrow", &g_Options.Visuals.Players.EnemyArrow);
					widgets->checkbox("Distance Rings", &g_Options.Visuals.Players.DistanceRings);
					widgets->checkbox("Scan Line", &g_Options.Visuals.Players.ScanLine);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Colors##extras", 2);
				{
					widgets->coloredit("Snap Line Color", g_Options.Visuals.Players.SnapLineColor);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});
#endif

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		// --- Tab 2: Lists ---
		widgets->nav.addpage(2, [&] {
			static int selectedNetId = -1;
			static char listSearch[64] = "";

			ImGui::BeginGroup();
			{
				widgets->child.begin("Player List", 1);
				{
					widgets->textinput("Filter", listSearch, sizeof(listSearch));

					auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();
					auto entities = Cheat::g_Fivem.GetEntitiyList();

					ImGui::BeginChild("playerlist", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
					for (const auto& entity : entities)
					{
						if (entity.StaticInfo.bIsLocalPlayer)
							continue;
						if (entity.StaticInfo.bIsNPC)
							continue;

						std::string name = entity.NetworkInfo.UserName;
						if (name.empty() || name == "Unknown")
							name = entity.StaticInfo.Name.empty() ? "Unknown" : entity.StaticInfo.Name;
						if (listSearch[0] != '\0' && name.find(listSearch) == std::string::npos)
							continue;

						float distance = localInfo.Ped ? entity.Cordinates.DistTo(localInfo.WorldPos) : 0.f;
						std::string row = name + "  |  " + std::to_string((int)distance) + "m";
						if (entity.StaticInfo.IsFriend)
							row += "  [Friend]";

						ImGui::PushID(entity.StaticInfo.NetId);
						bool selected = selectedNetId == entity.StaticInfo.NetId;
						if (ImGui::Selectable(row.c_str(), selected))
							selectedNetId = entity.StaticInfo.NetId;
						ImGui::PopID();
					}
					ImGui::EndChild();
				}
				widgets->child.end();
			}
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			{
				widgets->child.begin("Selected Target", 2);
				{
					if (selectedNetId < 0)
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.f), "Select a player from the list.");
					}
					else
					{
						const Cheat::Entity* selectedEntity = nullptr;
						for (const auto& entity : Cheat::g_Fivem.GetEntitiyList())
						{
							if (entity.StaticInfo.NetId == selectedNetId)
							{
								selectedEntity = &entity;
								break;
							}
						}

						if (!selectedEntity)
						{
							ImGui::TextColored(ImVec4(0.9f, 0.35f, 0.35f, 1.f), "Player no longer available.");
							selectedNetId = -1;
						}
						else
						{
							auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();
							float distance = localInfo.Ped ? selectedEntity->Cordinates.DistTo(localInfo.WorldPos) : 0.f;

							ImGui::Text("%s", selectedEntity->StaticInfo.Name.c_str());
							ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.f), "Distance: %.0fm", distance);
							ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.f), "Net ID: %d", selectedEntity->StaticInfo.NetId);
							ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.f), "Friend: %s", selectedEntity->StaticInfo.IsFriend ? "Yes" : "No");
						}
					}
				}
				widgets->child.end();

				widgets->child.begin("Actions", 3);
				{
					if (selectedNetId >= 0)
					{
						const Cheat::Entity* selectedEntity = nullptr;
						for (const auto& entity : Cheat::g_Fivem.GetEntitiyList())
						{
							if (entity.StaticInfo.NetId == selectedNetId)
							{
								selectedEntity = &entity;
								break;
							}
						}

						if (selectedEntity)
						{
							auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();

							if (widgets->button("Teleport To", ImVec2(0, 30)))
							{
								auto ped = localInfo.Ped;
								if (ped)
								{
									Vector3D tp = selectedEntity->Cordinates;
									tp.z += 1.f;
									Cheat::g_Fivem.TeleportToObject(
										(uint64_t)ped,
										ped->GetNavigation(),
										ped->GetModelInfo(),
										tp, tp, true);
								}
							}

							const char* friendLabel = selectedEntity->StaticInfo.IsFriend ? "Remove Friend" : "Add Friend";
							if (widgets->button(friendLabel, ImVec2(0, 30)))
							{
								if (selectedEntity->StaticInfo.IsFriend)
									Cheat::g_Fivem.FriendList.erase(selectedNetId);
								else
									Cheat::g_Fivem.FriendList[selectedNetId] = selectedEntity->StaticInfo;
							}
						}
					}
					else
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.f), "No target selected.");
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

		widgets->nav.addpage(2, [&] {
			static int selectedVehicle = -1;
			static char vehicleSearch[64] = "";

			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle List", 1);
				{
					widgets->textinput("Filter", vehicleSearch, sizeof(vehicleSearch));

					auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();
					auto vehicles = Cheat::g_Fivem.GetVehicleList();

					ImGui::BeginChild("vehiclelist", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
					for (size_t i = 0; i < vehicles.size(); ++i)
					{
						const auto& vehicle = vehicles[i];
						if (vehicleSearch[0] != '\0' && vehicle.Name.find(vehicleSearch) == std::string::npos)
							continue;

						float distance = localInfo.Ped ? vehicle.Vehicle->GetCoordinate().DistTo(localInfo.WorldPos) : 0.f;
						std::string row = vehicle.Name + "  |  " + std::to_string((int)distance) + "m";

						ImGui::PushID((int)i);
						bool selected = selectedVehicle == (int)i;
						if (ImGui::Selectable(row.c_str(), selected))
							selectedVehicle = (int)i;
						ImGui::PopID();
					}
					ImGui::EndChild();
				}
				widgets->child.end();
			}
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			{
				widgets->child.begin("Selected Target", 2);
				{
					auto vehicles = Cheat::g_Fivem.GetVehicleList();
					if (selectedVehicle < 0 || selectedVehicle >= (int)vehicles.size())
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.f), "Select a vehicle from the list.");
					}
					else
					{
						const auto& vehicle = vehicles[selectedVehicle];
						auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();
						float distance = localInfo.Ped ? vehicle.Vehicle->GetCoordinate().DistTo(localInfo.WorldPos) : 0.f;

						ImGui::Text("%s", vehicle.Name.c_str());
						ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.7f, 1.f), "Distance: %.0fm", distance);
					}
				}
				widgets->child.end();

				widgets->child.begin("Actions", 3);
				{
					auto vehicles = Cheat::g_Fivem.GetVehicleList();
					if (selectedVehicle >= 0 && selectedVehicle < (int)vehicles.size())
					{
						const auto& vehicle = vehicles[selectedVehicle];
						auto localInfo = Cheat::g_Fivem.GetLocalPlayerInfo();

						if (widgets->button("Teleport Vehicle", ImVec2(0, 30)))
						{
							auto ped = localInfo.Ped;
							if (ped && vehicle.Vehicle)
							{
								Vector3D tp = vehicle.Vehicle->GetCoordinate();
								tp.z += 1.f;
								Cheat::g_Fivem.TeleportToObject(
									(uint64_t)ped,
									ped->GetNavigation(),
									ped->GetModelInfo(),
									tp, tp, true);
							}
						}
					}
					else
					{
						ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.f), "No target selected.");
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});
#endif

		// --- Tab 2/3: Exploits / Features ---
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
#define TRINITY_TAB_FEATURES 3
#define TRINITY_TAB_SETTINGS 4
#else
#define TRINITY_TAB_FEATURES 2
#define TRINITY_TAB_SETTINGS 3
#endif
		// Subtab 0 – Self
		widgets->nav.addpage(TRINITY_TAB_FEATURES, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Self", 10);
				{
					widgets->checkbox("God Mode", &g_Options.Exploits.Self.GodMode);
					widgets->binder("GodMode Key", &g_Options.Exploits.Self.GodKey);
					widgets->checkbox("No Clip", &g_Options.Exploits.Self.NoClip);
					widgets->binder("Keybind##noclip", &g_Options.Exploits.Self.NoClipKey);
					widgets->combo("NoClip Mode", &g_Options.Exploits.Self.NoClipMode, { "Normal", "Invisible" });
					widgets->checkbox("Fake Lag", &g_Options.Exploits.Self.SpinBot);
					widgets->sliderfloat("Lag Range", &g_Options.Exploits.Self.SpinBotSpeed, 1.f, 50.f);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
				widgets->child.begin("Movement", 11);
				{
					widgets->checkbox("No Ragdoll", &g_Options.Exploits.Self.NoRagDoll);
					widgets->checkbox("Auto Heal", &g_Options.Exploits.Self.AutoHeal);
					widgets->checkbox("Auto Armor", &g_Options.Exploits.Self.AutoArmor);
				}
				widgets->child.end();
#else
				widgets->child.begin("Misc##self", 11);
				{
					widgets->checkbox("No Ragdoll", &g_Options.Exploits.Self.NoRagDoll);
					widgets->checkbox("Seatbelt", &g_Options.Exploits.Self.SeatBelt);
					widgets->checkbox("Vehicle Repair", &g_Options.Exploits.Self.VehicleRepair);
					widgets->binder("Repair Key", &g_Options.Exploits.Self.VehicleRepairKey);
				}
				widgets->child.end();
#endif
			}
			ImGui::EndGroup();
			});

		// Subtab 1 – Weapon
		widgets->nav.addpage(TRINITY_TAB_FEATURES, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Weapon", 10);
				{
					widgets->checkbox("Remove Spread", &g_Options.Exploits.Self.RemoveSpread);
					widgets->checkbox("Remove Recoil", &g_Options.Exploits.Self.RemoveRecoil);
					widgets->sliderfloat("Weapon Size", &g_Options.Exploits.Self.WeaponSize, 0.1f, 100.f);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		// Subtab 2 – Vehicle
		widgets->nav.addpage(TRINITY_TAB_FEATURES, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle", 10);
				{
					widgets->checkbox("Vehicle Boost", &g_Options.Exploits.Self.VehicleBoost);
					widgets->binder("Boost Key", &g_Options.Exploits.Self.VehicleBoostKey);
					widgets->sliderfloat("Boost Multiplier", &g_Options.Exploits.Self.VehicleBoostMultiplier, 1.f, 10.f);
					widgets->checkbox("Vehicle Repair", &g_Options.Exploits.Self.VehicleRepair);
					widgets->binder("Repair Key", &g_Options.Exploits.Self.VehicleRepairKey);
					widgets->checkbox("Seatbelt", &g_Options.Exploits.Self.SeatBelt);
#if defined(TRINITY_DEV) && TRINITY_DEV
					widgets->textinput("Plate Text", g_Options.Exploits.Self.PlateText, sizeof(g_Options.Exploits.Self.PlateText));
					widgets->checkbox("Plate Changer", &g_Options.Exploits.Self.PlateChanger);
					if (widgets->button("Apply Plate"))
						g_Options.Exploits.Self.PlateApplyPending = true;
					widgets->binder("Apply Plate Key", &g_Options.Exploits.Self.PlateChangerKey);
					ImGui::TextDisabled("Copies oxplate command. F8 paste, or oxplatem OLD NEW if not in vehicle.");
#endif
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("Vehicle Actions", 11);
				{
					widgets->checkbox("Steal Vehicle", &g_Options.Exploits.Self.StealVehicle);
					widgets->checkbox("Unlock All Vehicles", &g_Options.Exploits.Self.UnLockAllVehicles);
					widgets->checkbox("Lock All Vehicles", &g_Options.Exploits.Self.LockAllVehicles);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});
#endif

		// Subtab 2/3 – Others / Misc
		widgets->nav.addpage(TRINITY_TAB_FEATURES, [&] {
			ImGui::BeginGroup();
			{
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
				widgets->child.begin("Misc", 10);
				{
					if (widgets->checkbox("Stream Proof", &g_Options.General.CaptureBypass))
						FrameWork::Overlay::ApplyStreamProof(g_Options.General.CaptureBypass);
				}
				widgets->child.end();
#else
				widgets->child.begin("Others", 10);
				{
					if (widgets->checkbox("Stream Proof", &g_Options.General.CaptureBypass))
						FrameWork::Overlay::ApplyStreamProof(g_Options.General.CaptureBypass);
					widgets->binder("Menu Key", &g_Options.General.MenuKey);

					widgets->checkbox(BRAND_CLEAN_TRACES, &g_Options.General.CleanTraces);

					if (widgets->button("Unload", ImVec2(0, 30)))
					{
						if (g_Options.General.CleanTraces)
							LocalConfig::CleanDataDirectory();

						g_Options.General.ShutDown = true;
					}
				}
				widgets->child.end();
#endif
			}
			ImGui::EndGroup();
			});

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		widgets->nav.addpage(TRINITY_TAB_SETTINGS, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("##settings_left", 1);
				{
					widgets->trinity_section("Display");
					if (widgets->checkbox("Stream Proof", &g_Options.General.CaptureBypass))
						FrameWork::Overlay::ApplyStreamProof(g_Options.General.CaptureBypass);

					widgets->trinity_divider();
					widgets->trinity_section("Interface");
					widgets->checkbox("Show Binds", &g_Options.General.ShowKeybindList);

					widgets->spacing(8 ds);
					widgets->spacing(10 ds);
					if (widgets->button("Unload", ImVec2(-1.f, 34.f), { .style = button_outline, .rounding = 8.f }))
					{
						if (g_Options.General.CleanTraces)
							LocalConfig::CleanDataDirectory();

						g_Options.General.ShutDown = true;
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			ImGui::SameLine();
			ImGui::BeginGroup();
			{
				widgets->child.begin("##settings_right", 2);
				{
					widgets->trinity_section("Options");
					widgets->trinity_setting_row("Menu Key", [&] {
						widgets->binder("##menu_key", &g_Options.General.MenuKey);
					});
					widgets->checkbox(BRAND_CLEAN_TRACES, &g_Options.General.CleanTraces);
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
			});
#endif

		// --- Settings: Configs ---
		widgets->nav.addpage(TRINITY_TAB_SETTINGS, [&] {
			ImGui::BeginGroup();
			{
				widgets->child.begin("Create Config", 1);
				{
					static char configNameBuffer[64] = "";
					static int configType = 0;
					static std::string statusMsg;
					static std::string statusConfigName;
					static int statusConfigType = 0;
					static float statusMsgAlpha = 0.f;
					static float statusMsgTimer = 0.f;
					static bool statusMsgSuccess = true;

					widgets->textinput("Config Name", configNameBuffer, sizeof(configNameBuffer));
					widgets->combo("Type", &configType, { "Camp", "Rage", "Legit", "Roleplay" });

					if (widgets->button("Save Config", ImVec2(0, 30)))
					{
						if (strlen(configNameBuffer) > 0)
						{
							std::string tempMsg;
							bool success = LocalConfig::SaveConfig(std::string(configNameBuffer), (LocalConfig::ConfigType)configType, tempMsg);
							if (success)
							{
								AddConfigNotification("Config saved successfully!");
								memset(configNameBuffer, 0, sizeof(configNameBuffer));
							}
						}
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();

			ImGui::SameLine();

			ImGui::BeginGroup();
			{
				widgets->child.begin("Load Config", 2);
				{
					static std::map<std::string, float> hoverAnimations;
					static std::map<std::string, float> scaleAnimations;

					// Get config list
					auto configs = LocalConfig::GetConfigList();

					if (configs.empty())
					{
						ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 0.7f), "No configs found");
					}
					else
					{
						// Render each config
						for (auto& cfg : configs)
						{
							ImGui::PushID(cfg.filename.c_str());

							// Initialize animations
							if (hoverAnimations.find(cfg.filename) == hoverAnimations.end())
								hoverAnimations[cfg.filename] = 0.f;
							if (scaleAnimations.find(cfg.filename) == scaleAnimations.end())
								scaleAnimations[cfg.filename] = 0.f;

							float& hoverAnim = hoverAnimations[cfg.filename];
							float& scaleAnim = scaleAnimations[cfg.filename];

							// Config container with darker background
							ImVec2 cursorPos = ImGui::GetCursorScreenPos();
							float baseHeight = 70.f;
							float expandAmount = 6.f; // Subtle expansion
							float currentHeight = baseHeight + (expandAmount * scaleAnim);
							ImVec2 containerSize = ImVec2(ImGui::GetContentRegionAvail().x, currentHeight);

							// Check if hovered
							bool hovered = ImGui::IsMouseHoveringRect(cursorPos, ImVec2(cursorPos.x + containerSize.x, cursorPos.y + containerSize.y));

							// Update animations
							hoverAnim = ImLerp(hoverAnim, hovered ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 10.f);
							scaleAnim = ImLerp(scaleAnim, hovered ? 1.f : 0.f, ImGui::GetIO().DeltaTime * 12.f);

							// Depth effect - shadow layers when hovered (drawn BEFORE main background)
							if (hoverAnim > 0.01f)
							{
								// Shadow layer 1 (furthest) - offset 3px
								ImGui::GetWindowDrawList()->AddRectFilled(
									ImVec2(cursorPos.x + 3, cursorPos.y + 3),
									ImVec2(cursorPos.x + containerSize.x + 3, cursorPos.y + containerSize.y + 3),
									ImColor(0.f, 0.f, 0.f, 0.12f * hoverAnim),
									6.f
								);

								// Shadow layer 2 (middle) - offset 2px
								ImGui::GetWindowDrawList()->AddRectFilled(
									ImVec2(cursorPos.x + 2, cursorPos.y + 2),
									ImVec2(cursorPos.x + containerSize.x + 2, cursorPos.y + containerSize.y + 2),
									ImColor(0.f, 0.f, 0.f, 0.18f * hoverAnim),
									6.f
								);

								// Shadow layer 3 (closest) - offset 1px
								ImGui::GetWindowDrawList()->AddRectFilled(
									ImVec2(cursorPos.x + 1, cursorPos.y + 1),
									ImVec2(cursorPos.x + containerSize.x + 1, cursorPos.y + containerSize.y + 1),
									ImColor(0.f, 0.f, 0.f, 0.25f * hoverAnim),
									6.f
								);
							}

							// Background with smooth color transition
							float bgBrightness = 0.05f + (0.04f * hoverAnim); // Subtle brightness increase
							ImColor bgColor = ImColor(bgBrightness, bgBrightness, bgBrightness, 1.f);

							// Draw main background
							ImGui::GetWindowDrawList()->AddRectFilled(
								cursorPos,
								ImVec2(cursorPos.x + containerSize.x, cursorPos.y + containerSize.y),
								bgColor,
								6.f
							);

							// Subtle border glow on hover
							if (hoverAnim > 0.01f)
							{
								ImGui::GetWindowDrawList()->AddRect(
									cursorPos,
									ImVec2(cursorPos.x + containerSize.x, cursorPos.y + containerSize.y),
									ImColor(1.f, 1.f, 1.f, 0.1f * hoverAnim),
									6.f,
									0,
									1.5f
								);
							}

							// Left click to load
							if (hovered && ImGui::IsMouseClicked(0))
							{
								std::string tempMsg;
								bool success = LocalConfig::LoadConfig(cfg.filename, tempMsg);
								if (success)
								{
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
									g_ActiveConfigName = cfg.name;
#endif
									AddConfigNotification("Config loaded successfully!");
								}
							}

							// Right click to delete
							if (hovered && ImGui::IsMouseClicked(1))
							{
								std::string tempMsg;
								LocalConfig::DeleteConfig(cfg.filename, tempMsg);
							}

							// Type indicator (colored circle) - top right
							ImVec4 typeColor;
							const char* typeName;
							switch (cfg.type)
							{
							case LocalConfig::TYPE_CAMP:
								typeColor = ImVec4(0.3f, 1.f, 0.3f, 1.f);
								typeName = "Camp";
								break;
							case LocalConfig::TYPE_RAGE:
								typeColor = ImVec4(1.f, 0.3f, 0.3f, 1.f);
								typeName = "Rage";
								break;
							case LocalConfig::TYPE_LEGIT:
								typeColor = ImVec4(0.3f, 0.6f, 1.f, 1.f);
								typeName = "Legit";
								break;
							case LocalConfig::TYPE_ROLEPLAY:
								typeColor = ImVec4(1.f, 0.8f, 0.3f, 1.f);
								typeName = "Roleplay";
								break;
							}

							// Circle with glow on hover
							ImVec2 circlePos = ImVec2(cursorPos.x + containerSize.x - 20, cursorPos.y + 20);
							if (hoverAnim > 0.01f)
							{
								ImGui::GetWindowDrawList()->AddCircleFilled(circlePos, 8.f, ImColor(typeColor.x, typeColor.y, typeColor.z, 0.3f * hoverAnim), 16);
							}
							ImGui::GetWindowDrawList()->AddCircleFilled(circlePos, 6.f, ImColor(typeColor), 16);

							// Config name - left side with smooth color
							float textBrightness = 0.9f + (0.1f * hoverAnim);
							ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 12, cursorPos.y + 12 + (expandAmount * scaleAnim * 0.2f)));
							ImGui::TextColored(ImVec4(textBrightness, textBrightness, textBrightness, 0.9f), "%s", cfg.name.c_str());

							// Type text - below name
							ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 12, cursorPos.y + 32 + (expandAmount * scaleAnim * 0.4f)));
							ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 0.7f + 0.1f * hoverAnim), "Type: %s", typeName);

							// Date - bottom left
							ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + 12, cursorPos.y + 50 + (expandAmount * scaleAnim * 0.6f)));
							ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 0.6f + 0.1f * hoverAnim), "%s", cfg.date.c_str());

							// Hover hint with fade-in
							if (hoverAnim > 0.01f)
							{
								ImGui::SetCursorScreenPos(ImVec2(cursorPos.x + containerSize.x - 150, cursorPos.y + containerSize.y - 20));
								ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.8f * hoverAnim), "LMB: Load | RMB: Delete");
							}

							// Move cursor for next item
							ImGui::SetCursorScreenPos(ImVec2(cursorPos.x, cursorPos.y + containerSize.y + 8));

							ImGui::PopID();
						}
					}
				}
				widgets->child.end();
			}
			ImGui::EndGroup();
		});
		
#if defined(TRINITY_DEV) && TRINITY_DEV
#define TRINITY_TAB_DEV 5
		Cheat::DevPanel::RegisterPages(TRINITY_TAB_DEV);
#endif

		// Initialize ESP Preview items
		g_esppreview->create_text("Distance", "25m", esppos_top, &g_Options.Visuals.Players.EnableDistance, g_Options.Visuals.Players.BoxColor);
		g_esppreview->create_text("Weapon", "Five-Seven", esppos_bottom, &g_Options.Visuals.Players.WeaponName, g_Options.Visuals.Players.BoxColor);
		g_esppreview->create_bar("Health", esppos_left, &g_Options.Visuals.Players.HealthBar, g_Options.Visuals.Players.HealthBarColor, nullptr);
		g_esppreview->create_bar("Armor", esppos_right, &g_Options.Visuals.Players.AmorBar, g_Options.Visuals.Players.BoxColor, nullptr);
		
		// Initialize local config system
		LocalConfig::Initialize();
	}

	void Interface::UpdateStyle()
	{
		g_style->setup();
	}

	void Interface::RenderGui()
	{
		using namespace ImGui;

		widgets->binder_capturing = false;

		// Always render watermark
		RenderWatermark();
		
		if (!bIsMenuOpen)
			return;

		if (GetIO().DisplaySize.x < 64.f || GetIO().DisplaySize.y < 64.f)
			return;

		static int s_guiFrames = 0;
		if (s_guiFrames < 8)
		{
			++s_guiFrames;
			const ImVec2 displaySize = GetIO().DisplaySize;
			SetNextWindowPos(ImVec2(displaySize.x * 0.5f - 520.f, displaySize.y * 0.5f - 326.f), ImGuiCond_Always);
			SetNextWindowSize(ImVec2(1040.f, 652.f), ImGuiCond_Always);
			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			Begin(BRAND_MENU_WINDOW, nullptr,
				ImGuiWindowFlags_NoTitleBar |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoBringToFrontOnFocus);
			PopStyleVar();
			ImDrawList* dl = GetWindowDrawList();
			const ImVec2 wpos = GetWindowPos();
			const ImVec2 wsz = GetWindowSize();
			dl->AddRectFilled(wpos, wpos + wsz, IM_COL32(18, 18, 18, 255), 12.f);
			dl->AddRect(wpos, wpos + wsz, IM_COL32(255, 255, 255, 16), 12.f);
			const char* title = BRAND_APP_NAME;
			const ImVec2 ts = CalcTextSize(title);
			dl->AddText(wpos + ImVec2{ (wsz.x - ts.x) * 0.5f, (wsz.y - ts.y) * 0.5f }, IM_COL32(245, 245, 247, 255), title);
			End();
			return;
		}

		// Check if we're on Visuals tab (tab 1) AND Visible subtab (subtab 0)
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		const bool bShowEspPreview = false;
#else
		bool bShowEspPreview = (widgets->nav.current == 1 && widgets->nav.tabs[1].current == 0);
#endif
		
		// Animate ESP preview panel
		fEspPreviewAnim = ImLerp(fEspPreviewAnim, bShowEspPreview ? 1.f : 0.f, GetIO().DeltaTime * 8.f);

		// Calculate menu width with ESP preview
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		const float baseMenuWidth = 1040.f;
		const float menuHeight = 652.f;
#elif defined(BRAND_MODERN_UI)
		float baseMenuWidth = 860.f;
		float menuHeight = 520.f;
#else
		float baseMenuWidth = 744.f;
		float menuHeight = 464.f;
#endif
		float espPreviewWidth = 360.f;
		float totalWidth = baseMenuWidth + (espPreviewWidth * fEspPreviewAnim);

		// Center the menu on screen (only first time)
		ImVec2 displaySize = GetIO().DisplaySize;
		SetNextWindowPos(ImVec2(displaySize.x * 0.5f - totalWidth * 0.5f, displaySize.y * 0.5f - menuHeight * 0.5f), ImGuiCond_FirstUseEver);
		SetNextWindowSize(ImVec2(totalWidth, menuHeight), ImGuiCond_Always);

		// Use custom flags to allow moving while keeping no title bar
		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		Begin(BRAND_MENU_WINDOW, nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBringToFrontOnFocus);
		PopStyleVar();
		{
			ImDrawList* dl = GetWindowDrawList();
			ImVec2 wpos    = GetWindowPos();
			ImVec2 wsz     = GetWindowSize();

#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
			if (widgets->nav.current < 0 || widgets->nav.current >= (int)widgets->nav.tabs.size())
				widgets->nav.current = 0;
			if (widgets->nav.next < 0 || widgets->nav.next >= (int)widgets->nav.tabs.size())
				widgets->nav.next = widgets->nav.current;

			slate->sidebar_anim = 1.f;
			const float headerH = 72.f;
			const float subtabH = 36.f;
			const float footerH = 32.f;
			const float round = 12.f;
			const ImU32 windowBg = IM_COL32(18, 18, 18, 255);
			const ImU32 chromeBg = IM_COL32(20, 20, 20, 255);
			const ImU32 dividerCol = IM_COL32(255, 255, 255, 14);
			static DWORD gamePid = 0;
			static int playerCount = 0;
			static int npcCount = 0;
			static int vehicleCount = 0;
			static float statsTimer = 0.f;
			statsTimer -= GetIO().DeltaTime;
			if (statsTimer <= 0.f)
			{
				statsTimer = 2.f;
				gamePid = FrameWork::Memory::GetFiveMGTAProcessPid();
				playerCount = 0;
				npcCount = 0;
				vehicleCount = 0;
				if (Cheat::g_Fivem.IsInitialized())
				{
					Cheat::g_Fivem.LockLists.lock();
					for (const auto& entity : Cheat::g_Fivem.GetEntitiyList())
					{
						if (entity.StaticInfo.bIsLocalPlayer)
							continue;
						if (entity.StaticInfo.bIsNPC)
							++npcCount;
						else
							++playerCount;
					}
					Cheat::g_Fivem.LockLists.unlock();
					vehicleCount = (int)Cheat::g_Fivem.GetVehicleList().size();
				}
			}

			dl->AddRectFilled(wpos, wpos + wsz, windowBg, round);
			dl->AddRect(wpos, wpos + wsz, IM_COL32(255, 255, 255, 16), round);
			dl->AddRectFilled(wpos, wpos + ImVec2{ wsz.x, headerH }, chromeBg, round, ImDrawFlags_RoundCornersTop);
			dl->AddLine(wpos + ImVec2{ 0.f, headerH }, wpos + ImVec2{ wsz.x, headerH }, dividerCol);
			dl->AddLine(wpos + ImVec2{ 0.f, headerH + subtabH }, wpos + ImVec2{ wsz.x, headerH + subtabH }, dividerCol);

			{
				const auto& navTab = widgets->nav.tabs[widgets->nav.current];
				std::string title(navTab.label.translate());
				std::string sub;
				if (!navTab.subtabs.empty())
				{
					int subIdx = navTab.current;
					if (subIdx < 0 || subIdx >= (int)navTab.subtabs.size())
						subIdx = 0;
					sub = std::string(navTab.subtabs[subIdx].label.translate());
				}

				dl->AddText(wpos + ImVec2{ 22.f, 14.f }, IM_COL32(245, 245, 247, 255), title.c_str());

				float crumbX = wpos.x + 22.f;
				const float crumbY = wpos.y + 36.f;
				dl->AddText(ImVec2{ crumbX, crumbY }, IM_COL32(140, 146, 158, 255), title.c_str());
				crumbX += CalcTextSize(title.c_str()).x;
				if (!sub.empty())
				{
					dl->AddText(ImVec2{ crumbX, crumbY }, IM_COL32(90, 96, 108, 255), "  >  ");
					crumbX += CalcTextSize("  >  ").x;
					dl->AddText(ImVec2{ crumbX, crumbY }, IM_COL32(156, 163, 175, 255), sub.c_str());
				}

				const std::string userName = g_Options.General.DiscordUsername.empty() ? std::string("User") : g_Options.General.DiscordUsername;
#if defined(HZ_DEV) && HZ_DEV
				const char* role = "Developer";
#elif defined(FRIENDS_BUILD) && FRIENDS_BUILD
				const char* role = BRAND_USER_ROLE;
#else
				const char* role = BRAND_USER_ROLE;
#endif
				const float userW = HzUserChipWidth(userName.c_str(), role);
				HzUserChip(dl, ImVec2{ wpos.x + wsz.x - userW - 22.f, wpos.y + 18.f }, userName.c_str(), role, DiscordAvatarTexture);
			}

			const float iconRowW = (float)widgets->nav.tabs.size() * 46.f - 6.f;
			SetCursorPos(ImVec2{ (wsz.x - iconRowW) * 0.5f, 16.f });
			widgets->nav.drawtabs_hz();

			SetCursorPos(ImVec2{ 16.f, headerH });
			widgets->nav.drawsubtabs_trinity();

			PushStyleVar(ImGuiStyleVar_Alpha, GImGui->Style.Alpha * widgets->nav.tab_anim * widgets->nav.subtab_anim);
			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 16.f, 16.f });
			PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 12.f, 12.f });
			SetCursorPos(ImVec2{ 0.f, headerH + subtabH });
			const float pageH = ImMax(120.f, wsz.y - headerH - subtabH - footerH);
			BeginChild("page", { wsz.x, pageH }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				widgets->child.smoothscroll();
				widgets->nav.drawpage();
			}
			EndChild();
			PopStyleVar(3);

			{
				const int startedAt = g_Options.Exploits.Session.SessionStartTime;
				const int elapsed = startedAt > 0 ? ImMax(0, (int)time(0) - startedAt) : 0;

				char processText[64];
				if (gamePid)
					ImFormatString(processText, sizeof(processText), "GTAProcess.exe %lu", gamePid);
				else
					ImFormatString(processText, sizeof(processText), "not attached");

				char sessionText[32], pedsText[16], vehicleText[16], fpsText[32];
				ImFormatString(sessionText, sizeof(sessionText), "%02d:%02d:%02d", elapsed / 3600, (elapsed % 3600) / 60, elapsed % 60);
				ImFormatString(pedsText, sizeof(pedsText), "%d", playerCount + npcCount);
				ImFormatString(vehicleText, sizeof(vehicleText), "%d", vehicleCount);
				ImFormatString(fpsText, sizeof(fpsText), "%.0f fps · %.1f ms", GetIO().Framerate, 1000.f / ImMax(1.f, GetIO().Framerate));

				const float footerY = wpos.y + wsz.y - footerH;
				dl->AddRectFilled(ImVec2{ wpos.x, footerY }, wpos + wsz, chromeBg, round, ImDrawFlags_RoundCornersBottom);
				dl->AddLine(ImVec2{ wpos.x, footerY }, ImVec2{ wpos.x + wsz.x, footerY }, dividerCol);

				const float textY = footerY + 8.f;
				float leftX = wpos.x + 20.f;
				TrinityStatusItem(dl, ImVec2{ leftX, textY }, "ATTACHED", processText, gamePid ? IM_COL32(110, 190, 140, 255) : IM_COL32(180, 184, 196, 255));
				leftX += TrinityStatusWidth("ATTACHED", processText) + 18.f;
				TrinityStatusItem(dl, ImVec2{ leftX, textY }, "SESSION", sessionText);
				leftX += TrinityStatusWidth("SESSION", sessionText) + 18.f;
				TrinityStatusItem(dl, ImVec2{ leftX, textY }, "CONFIG", g_ActiveConfigName.c_str());

				float rightX = wpos.x + wsz.x - 20.f;
				rightX -= TrinityStatusWidth("OVERLAY", fpsText);
				TrinityStatusItem(dl, ImVec2{ rightX, textY }, "OVERLAY", fpsText);
				rightX -= 18.f + TrinityStatusWidth("VEH", vehicleText);
				TrinityStatusItem(dl, ImVec2{ rightX, textY }, "VEH", vehicleText);
				rightX -= 18.f + TrinityStatusWidth("PEDS", pedsText);
				TrinityStatusItem(dl, ImVec2{ rightX, textY }, "PEDS", pedsText);
			}
#elif defined(BRAND_MODERN_UI)
			const float headerH = 54.f;
			slate->sidebar_anim = 1.f;
			float sideW = 200.f;
			const ImU32 accentCol = IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255);
			const ImU32 panelTop = IM_COL32(14, 11, 24, 255);
			const ImU32 panelBottom = IM_COL32(8, 6, 14, 255);
			const ImU32 headerBg = IM_COL32(22, 17, 36, 255);
			const ImU32 sidebarBg = IM_COL32(18, 14, 30, 255);

			dl->AddRectFilledMultiColor(wpos, wpos + wsz, panelTop, panelTop, panelBottom, panelBottom);
			dl->AddRectFilled(wpos, ImVec2(wpos.x + wsz.x, wpos.y + headerH), headerBg, 8.f, ImDrawFlags_RoundCornersTop);
			dl->AddRectFilled(ImVec2(wpos.x, wpos.y + headerH - 2.f), ImVec2(wpos.x + wsz.x, wpos.y + headerH), accentCol);

			const float headlineX = wpos.x + 24.f;
			ImVec2 headlineSize = CalcTextSize(BRAND_MENU_HEADLINE);
			dl->AddText(ImVec2(headlineX, wpos.y + 14.f), accentCol, BRAND_MENU_HEADLINE);
			dl->AddText(ImVec2(headlineX + headlineSize.x + 10.f, wpos.y + 18.f), IM_COL32(160, 160, 175, 220), BRAND_MENU_TAGLINE);

			SetCursorPos(ImVec2(0.f, headerH));

			BeginChild("sidebar", ImVec2{ sideW, 0 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImVec2 sp = GetWindowPos(), ss = GetWindowSize();
				GetWindowDrawList()->AddRectFilled(sp, sp + ss, sidebarBg, 4.f, ImDrawFlags_RoundCornersLeft);
				GetWindowDrawList()->AddRectFilled(sp + ImVec2{ ss.x - 1, 0 }, sp + ss, g_style->col(pcol_border));
				SetCursorPosY(GetCursorPosY() + 16.f);
				widgets->nav.drawtabs();
			}
			EndChild();

			SameLine(0, 0);

			BeginChild("main", ImVec2{ baseMenuWidth - sideW, 0 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				widgets->nav.drawsubtabs();

				PushStyleVar(ImGuiStyleVar_Alpha, GImGui->Style.Alpha * widgets->nav.tab_anim * widgets->nav.subtab_anim);
				PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 20, 20 });
				PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 10, 10 });
				BeginChild("page", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				{
					widgets->child.smoothscroll();
					widgets->nav.drawpage();
				}
				EndChild();
				PopStyleVar(3);
			}
			EndChild();
#else
			dl->AddRectFilled(wpos, wpos + wsz, ImColor(10, 10, 10, 255), GImGui->Style.WindowRounding);
			float sideW = 74.f + (166.f - 74.f) * slate->sidebar_anim;
#endif

#if !defined(BRAND_TRINITY_UI) && !defined(BRAND_MODERN_UI)
			BeginChild("sidebar", ImVec2{ sideW, 0 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				ImVec2 sp = GetWindowPos(), ss = GetWindowSize();
				GetWindowDrawList()->AddRectFilled(sp, sp + ss, ImColor(14, 14, 14, 255), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersLeft);
				GetWindowDrawList()->AddRectFilled(sp + ImVec2{ ss.x - 1, 0 }, sp + ss, g_style->col(pcol_border));

				std::string displayName = g_Options.General.DiscordUsername.empty() ? XorStr("User") : g_Options.General.DiscordUsername;
				std::string displayRole = XorStr("injectdll") == displayName ? XorStr("Owner") : XorStr("User");

				ImVec2 nameSize = CalcTextSize(displayName.c_str());
				ImVec2 roleSize = CalcTextSize(displayRole.c_str());
				float yPos = GetCursorPosY() + 15;

				SetCursorPosX((sideW - nameSize.x) * 0.5f);
				SetCursorPosY(yPos);
				TextColored(ImVec4(1.f, 1.f, 1.f, 1.f), displayName.c_str());

				SetCursorPosX((sideW - roleSize.x) * 0.5f);
				SetCursorPosY(yPos + nameSize.y + 5);
				TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.f), displayRole.c_str());

				SetCursorPosY(yPos + nameSize.y + roleSize.y + 20);
				widgets->nav.drawtabs();
				slate->sidebar_anim = ImLerp(slate->sidebar_anim, IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) ? 1.f : 0.f, GetIO().DeltaTime * 8.f);
			}
			EndChild();

			SameLine(0, 0);

			BeginChild("main", ImVec2{ baseMenuWidth - sideW, 0 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			{
				widgets->nav.drawsubtabs();

				PushStyleVar(ImGuiStyleVar_Alpha, GImGui->Style.Alpha * widgets->nav.tab_anim * widgets->nav.subtab_anim);
				PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 20, 20 });
				PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 10, 10 });
				BeginChild("page", { 0, 0 }, ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				{
					widgets->child.smoothscroll();
					widgets->nav.drawpage();
				}
				EndChild();
				PopStyleVar(3);
			}
			EndChild();
#endif

#if !defined(BRAND_TRINITY_UI) || !BRAND_TRINITY_UI
			// ESP Preview Panel (animated)
			if (fEspPreviewAnim > 0.01f)
			{
				SameLine(0, 0);
				
				PushStyleVar(ImGuiStyleVar_Alpha, fEspPreviewAnim);
				BeginChild("esppreview", ImVec2{ espPreviewWidth * fEspPreviewAnim, 0 }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				{
					ImVec2 ep = GetWindowPos(), es = GetWindowSize();
					
					// Draw separator line
					GetWindowDrawList()->AddRectFilled(ep, ep + ImVec2{ 1, es.y }, g_style->col(pcol_border));
					
					// Draw ESP preview background
					GetWindowDrawList()->AddRectFilled(ep + ImVec2{ 1, 0 }, ep + es, ImColor(12, 12, 12, 255), GImGui->Style.WindowRounding, ImDrawFlags_RoundCornersRight);
					
					// Title
					SetCursorPos(ImVec2(20, 20));
					PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 0.9f));
					Text("ESP Preview");
					PopStyleColor();
					
					// Update bar positions based on user settings dynamically
					// We need to search through all items to update Health and Armor positions
					static e_esp_item_pos lastHealthPos = esppos_left;
					static e_esp_item_pos lastArmorPos = esppos_right;
					
					e_esp_item_pos currentHealthPos = (e_esp_item_pos)g_Options.Visuals.Players.HealthBarType;
					e_esp_item_pos currentArmorPos = (e_esp_item_pos)g_Options.Visuals.Players.AmorBarType;
					
					// Only update if positions changed to avoid constant resets
					if (lastHealthPos != currentHealthPos || lastArmorPos != currentArmorPos) {
						// Search through all items and update positions
						for (int pos = 0; pos < 4; pos++) {
							auto items_in_pos = g_esppreview->get_items_by_pos((e_esp_item_pos)pos);
							for (auto* item : items_in_pos) {
								if (item->name == "Health") {
									item->pos = currentHealthPos;
									item->posvec2 = ImVec2{0, 0}; // Reset animation
								}
								else if (item->name == "Armor") {
									item->pos = currentArmorPos;
									item->posvec2 = ImVec2{0, 0}; // Reset animation
								}
							}
						}
						lastHealthPos = currentHealthPos;
						lastArmorPos = currentArmorPos;
					}
					
					// Draw ESP preview in center
					ImVec2 previewCenter = ImVec2(ep.x + es.x * 0.5f, ep.y + es.y * 0.5f);
					
					// Call ESP preview draw function
					g_esppreview->draw(
						previewCenter,
						&g_Options.Visuals.Players.EnableBox,
						g_Options.Visuals.Players.BoxColor,
						&g_Options.Visuals.Players.BoxType,
						&g_Options.Visuals.Players.Skeleton,
						g_Options.Visuals.Players.SkeletonColor,
						nullptr, // chams_col
						nullptr  // chams_type
					);
					
					// After drawing, check if positions changed by dragging and update config
					for (int pos = 0; pos < 4; pos++) {
						auto items_in_pos = g_esppreview->get_items_by_pos((e_esp_item_pos)pos);
						for (auto* item : items_in_pos) {
							if (item->name == "Health" && item->pos != lastHealthPos) {
								g_Options.Visuals.Players.HealthBarType = (int)item->pos;
								lastHealthPos = item->pos;
							}
							else if (item->name == "Armor" && item->pos != lastArmorPos) {
								g_Options.Visuals.Players.AmorBarType = (int)item->pos;
								lastArmorPos = item->pos;
							}
						}
					}
				}
				EndChild();
				PopStyleVar();
			}
#endif
		}
		End();

		widgets->notify.handle();
		
		// Render config notifications
		RenderConfigNotifications();
		
		// Keybind hotkey processing
		for (int i = 0; i < 166; ++i)
			widgets->keybinds.keyshandle[i] = GetAsyncKeyState(i);
	}

	void Interface::RenderLoadingScreen()
	{
		ImGui::SetNextWindowSize(ImVec2(400, 200));
		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetIO().DisplaySize.x * 0.5f - 200,
			ImGui::GetIO().DisplaySize.y * 0.5f - 100));

		ImGui::Begin("##loading", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz  = ImGui::GetWindowSize();

			dl->AddRectFilled(pos, pos + sz, ImColor(16, 16, 16), 8.f);
			ImGui::SetCursorPos(ImVec2(sz.x * 0.5f - 50, sz.y * 0.5f - 10));
			ImGui::TextColored(ImVec4(1, 1, 1, 0.8f), "Loading... %.0f%%", fLoadingProgress * 100.f);
		}
		ImGui::End();
	}

	void Interface::RenderAuthScreen()
	{
		widgets->binder_capturing = false;

		if (!bIsMenuOpen)
			SetMenuOpen(true);

#if defined(LICENSE_AUTH) && LICENSE_AUTH
#if defined(BRAND_TRINITY_UI) && BRAND_TRINITY_UI
		ImGui::SetNextWindowSize(ImVec2(520, 368));
		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetIO().DisplaySize.x * 0.5f - 260,
			ImGui::GetIO().DisplaySize.y * 0.5f - 184));

		ImGui::Begin("##trinity_auth", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz  = ImGui::GetWindowSize();
			dl->AddRectFilled(pos, pos + sz, IM_COL32(18, 18, 18, 255), 12.f);
			dl->AddRect(pos, pos + sz, IM_COL32(255, 255, 255, 16), 12.f);
			dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + 64), IM_COL32(20, 20, 20, 255), 12.f, ImDrawFlags_RoundCornersTop);
			dl->AddRectFilled(ImVec2(pos.x, pos.y + 62), ImVec2(pos.x + sz.x, pos.y + 64), IM_COL32(BRAND_ACCENT_R, BRAND_ACCENT_G, BRAND_ACCENT_B, 255));

			ImGui::SetCursorPos(ImVec2(24, 16));
			ImGui::TextColored(ImVec4(0.96f, 0.96f, 0.97f, 1.f), BRAND_MENU_HEADLINE);
			ImGui::SetCursorPos(ImVec2(24, 36));
			ImGui::TextColored(ImVec4(0.55f, 0.57f, 0.62f, 1.f), "HZ  >  Sign in");

			ImGui::SetCursorPos(ImVec2(24, 84));
			ImGui::TextColored(ImVec4(0.96f, 0.96f, 0.97f, 1.f), BRAND_AUTH_TITLE);
			ImGui::SetCursorPos(ImVec2(24, 106));
			ImGui::PushTextWrapPos(sz.x - 24);
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.64f, 1.f), BRAND_AUTH_SUBTITLE);
			ImGui::PopTextWrapPos();

			ImGui::SetCursorPos(ImVec2(24, 144));
			ImGui::TextColored(ImVec4(0.72f, 0.74f, 0.78f, 1.f), BRAND_AUTH_KEY_LABEL);
			ImGui::SetCursorPos(ImVec2(24, 164));
			ImGui::SetNextItemWidth(sz.x - 48);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.10f, 0.10f, 0.11f, 1.f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.39f, 0.40f, 0.95f, 0.28f));
			ImGui::InputText("##friendskey", LicenseKeyBuffer, sizeof(LicenseKeyBuffer));
			ImGui::PopStyleColor(2);

			ImGui::SetCursorPos(ImVec2(24, 200));
			ImGui::TextColored(ImVec4(0.42f, 0.45f, 0.50f, 1.f), "Device  %s", HwidDisplayBuffer[0] ? HwidDisplayBuffer : "...");

			if (AuthErrorBuffer[0])
			{
				ImGui::SetCursorPos(ImVec2(24, 224));
				ImGui::PushTextWrapPos(sz.x - 24);
				ImGui::TextColored(ImVec4(1.f, 0.40f, 0.40f, 1.f), "%s", AuthErrorBuffer);
				ImGui::PopTextWrapPos();
			}

			bool authSuccess = false;
			std::string authError;
			Cheat::PollAuthUiState(authSuccess, authError, AuthBusy);
			if (authSuccess)
				bShowAuth = false;
			if (!authError.empty())
				strncpy_s(AuthErrorBuffer, authError.c_str(), _TRUNCATE);

			ImGui::SetCursorPos(ImVec2(24, 308));
			const float buttonWidth = (sz.x - 52.f) * 0.5f;
			if (AuthBusy)
			{
				ImGui::BeginDisabled();
				widgets->button(BRAND_AUTH_BUSY, ImVec2(buttonWidth, 36));
				ImGui::EndDisabled();
			}
			else if (widgets->button(BRAND_AUTH_LOGIN, ImVec2(buttonWidth, 36)))
			{
				AuthErrorBuffer[0] = '\0';
				Cheat::BeginAuthenticateWithKeyAsync(LicenseKeyBuffer);
				AuthBusy = true;
			}

			ImGui::SameLine(0.f, 12.f);
			if (AuthBusy)
			{
				ImGui::BeginDisabled();
				widgets->button(BRAND_AUTH_UNLOAD, ImVec2(buttonWidth, 36), { .style = button_outline, .rounding = 8.f });
				ImGui::EndDisabled();
			}
			else if (widgets->button(BRAND_AUTH_UNLOAD, ImVec2(buttonWidth, 36), { .style = button_outline, .rounding = 8.f }))
			{
				g_Options.General.ShutDown = true;
			}
		}
		ImGui::End();
#else
#if defined(BRAND_MODERN_UI)
		ImGui::SetNextWindowSize(ImVec2(500, 360));
		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetIO().DisplaySize.x * 0.5f - 250,
			ImGui::GetIO().DisplaySize.y * 0.5f - 180));

		ImGui::Begin("##pulse_auth", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz  = ImGui::GetWindowSize();
			dl->AddRectFilled(pos, pos + sz, ImColor(14, 11, 24), 10.f);
			dl->AddRectFilled(pos, ImVec2(pos.x + sz.x, pos.y + 56), ImColor(22, 17, 36), 10.f, ImDrawFlags_RoundCornersTop);
			dl->AddRectFilled(ImVec2(pos.x, pos.y + 54), ImVec2(pos.x + sz.x, pos.y + 56), ImColor(140, 82, 255, 255));

			ImGui::SetCursorPos(ImVec2(24, 16));
			ImGui::TextColored(ImVec4(0.55f, 0.32f, 1.f, 1.f), BRAND_MENU_HEADLINE);
			ImGui::SameLine(0.f, 8.f);
			ImGui::SetCursorPosY(20);
			ImGui::TextColored(ImVec4(0.65f, 0.65f, 0.72f, 1.f), BRAND_MENU_TAGLINE);

			ImGui::SetCursorPos(ImVec2(24, 72));
			ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.92f, 1.f), BRAND_AUTH_TITLE);
			ImGui::SetCursorPos(ImVec2(24, 94));
			ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.65f, 1.f), BRAND_AUTH_SUBTITLE);

			ImGui::SetCursorPos(ImVec2(24, 128));
			ImGui::TextColored(ImVec4(0.75f, 0.78f, 0.85f, 1.f), BRAND_AUTH_KEY_LABEL);
			ImGui::SetCursorPos(ImVec2(24, 150));
			ImGui::SetNextItemWidth(sz.x - 48);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.10f, 0.18f, 1.f));
			ImGui::InputText("##accesskey", LicenseKeyBuffer, sizeof(LicenseKeyBuffer));
			ImGui::PopStyleColor();

			ImGui::SetCursorPos(ImVec2(24, 178));
			ImGui::TextColored(ImVec4(0.5f, 0.53f, 0.6f, 1.f), "Device ID: %s", HwidDisplayBuffer[0] ? HwidDisplayBuffer : "...");
#else
		ImGui::SetNextWindowSize(ImVec2(440, 320));
		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetIO().DisplaySize.x * 0.5f - 220,
			ImGui::GetIO().DisplaySize.y * 0.5f - 160));

		ImGui::Begin("##auth", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz  = ImGui::GetWindowSize();
			dl->AddRectFilled(pos, pos + sz, ImColor(16, 16, 16), 8.f);

			ImGui::SetCursorPos(ImVec2(20, 18));
			ImGui::TextColored(ImVec4(1, 1, 1, 0.95f), BRAND_AUTH_TITLE);
			ImGui::SetCursorPos(ImVec2(20, 42));
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.f), BRAND_AUTH_SUBTITLE);

			ImGui::SetCursorPos(ImVec2(20, 72));
			ImGui::TextColored(ImVec4(1, 1, 1, 0.8f), BRAND_AUTH_KEY_LABEL);
			ImGui::SetCursorPos(ImVec2(20, 94));
			ImGui::SetNextItemWidth(sz.x - 40);
			ImGui::InputText("##licensekey", LicenseKeyBuffer, sizeof(LicenseKeyBuffer));

			ImGui::SetCursorPos(ImVec2(20, 128));
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.f), "HWID: %s", HwidDisplayBuffer[0] ? HwidDisplayBuffer : "...");
#endif

			if (AuthErrorBuffer[0])
			{
#if defined(BRAND_MODERN_UI)
				ImGui::SetCursorPos(ImVec2(24, 204));
#else
				ImGui::SetCursorPos(ImVec2(20, 154));
#endif
				ImGui::TextColored(ImVec4(1.f, 0.35f, 0.35f, 1.f), "%s", AuthErrorBuffer);
			}

			bool authSuccess = false;
			std::string authError;
			Cheat::PollAuthUiState(authSuccess, authError, AuthBusy);
			if (authSuccess)
				bShowAuth = false;
			if (!authError.empty())
				strncpy_s(AuthErrorBuffer, authError.c_str(), _TRUNCATE);

#if defined(BRAND_MODERN_UI)
			ImGui::SetCursorPos(ImVec2(24, 250));
#else
			ImGui::SetCursorPos(ImVec2(20, 210));
#endif
			const float buttonWidth = (sz.x - 52.f) * 0.5f;
			if (AuthBusy)
			{
				ImGui::BeginDisabled();
				widgets->button(BRAND_AUTH_BUSY, ImVec2(buttonWidth, 34));
				ImGui::EndDisabled();
			}
			else if (widgets->button(BRAND_AUTH_LOGIN, ImVec2(buttonWidth, 34)))
			{
				AuthErrorBuffer[0] = '\0';
				Cheat::BeginAuthenticateWithKeyAsync(LicenseKeyBuffer);
				AuthBusy = true;
			}

			ImGui::SameLine(0.f, 12.f);
			if (AuthBusy)
			{
				ImGui::BeginDisabled();
				widgets->button(BRAND_AUTH_UNLOAD, ImVec2(buttonWidth, 34));
				ImGui::EndDisabled();
			}
			else if (widgets->button(BRAND_AUTH_UNLOAD, ImVec2(buttonWidth, 34)))
			{
				g_Options.General.ShutDown = true;
			}
		}
		ImGui::End();
#endif
#else
		ImGui::SetNextWindowSize(ImVec2(400, 220));
		ImGui::SetNextWindowPos(ImVec2(
			ImGui::GetIO().DisplaySize.x * 0.5f - 200,
			ImGui::GetIO().DisplaySize.y * 0.5f - 110));

		ImGui::Begin("##auth", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetWindowPos();
			ImVec2 sz  = ImGui::GetWindowSize();
			dl->AddRectFilled(pos, pos + sz, ImColor(16, 16, 16), 8.f);

			ImGui::SetCursorPos(ImVec2(20, 20));
			ImGui::TextColored(ImVec4(1, 1, 1, 0.8f), "Discord ID:");
			ImGui::SetCursorPos(ImVec2(20, 45));
			ImGui::SetNextItemWidth(sz.x - 40);
			ImGui::InputText("##discordid", DiscordIDBuffer, sizeof(DiscordIDBuffer));

			ImGui::SetCursorPos(ImVec2(sz.x * 0.5f - 60, 100));
			if (widgets->button("Authenticate", ImVec2(120, 32)))
			{
				g_Options.General.DiscordID = DiscordIDBuffer;
				bShowAuth = false;
			}
		}
		ImGui::End();
#endif
	}

	void Interface::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
	}

	void Interface::ApplyMenuWindowStyle()
	{
		if (!hWindow)
			return;

		const bool secondMonitor = g_Options.General.SecondMonitor && g_Options.General.MonitorIndex >= 0;

		LONG desiredEx = WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED;
		if (!bIsMenuOpen)
		{
			desiredEx |= WS_EX_NOACTIVATE;
			if (!secondMonitor)
				desiredEx |= WS_EX_TRANSPARENT;
		}

		const LONG currentEx = GetWindowLong(hWindow, GWL_EXSTYLE);
		const bool styleChanged = currentEx != desiredEx;

		static bool s_lastSecondMonitor = false;
		const bool modeChanged = secondMonitor != s_lastSecondMonitor;
		s_lastSecondMonitor = secondMonitor;

		if (styleChanged)
			SetWindowLong(hWindow, GWL_EXSTYLE, desiredEx);

		if (modeChanged)
		{
			if (secondMonitor)
			{
				const MARGINS margins = { 0, 0, 0, 0 };
				DwmExtendFrameIntoClientArea(hWindow, &margins);
			}
			else
			{
				const MARGINS margins = { -1, -1, -1, -1 };
				DwmExtendFrameIntoClientArea(hWindow, &margins);
				SetLayeredWindowAttributes(hWindow, RGB(0, 0, 0), 255, LWA_ALPHA);
			}
		}

		if (styleChanged || modeChanged)
		{
			SetWindowPos(hWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			SetLayeredWindowAttributes(hWindow, RGB(0, 0, 0), 255, LWA_ALPHA);
			FrameWork::Overlay::ApplyCaptureBypass();
		}
	}

	void Interface::RefreshWindowStyle()
	{
		ApplyMenuWindowStyle();
	}

	void Interface::SetMenuOpen(bool open)
	{
		if (bIsMenuOpen == open)
			return;

		bIsMenuOpen = open;
		ApplyMenuWindowStyle();
	}

	void Interface::HandleMenuKey()
	{
		if (bShowAuth)
			return;

		if (widgets->binder_capturing)
			return;

		if (ImGui::GetIO().WantTextInput)
			return;

		static bool MenuKeyDown = false;
		const int menuKey = g_Options.General.MenuKey;
		if (menuKey <= 0)
		{
			MenuKeyDown = false;
			return;
		}

		if (GetAsyncKeyState(menuKey) & 0x8000)
		{
			if (!MenuKeyDown)
			{
				MenuKeyDown = true;
				SetMenuOpen(!bIsMenuOpen);
			}
		}
		else
		{
			MenuKeyDown = false;
		}
	}

	void Interface::ShutDown()
	{
		static bool imguiShutdownDone = false;
		if (imguiShutdownDone)
			return;
		imguiShutdownDone = true;

		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
	
	std::string Interface::GetKeyName(int key)
	{
		switch (key)
		{
		case VK_LBUTTON: return "LMB";
		case VK_RBUTTON: return "RMB";
		case VK_MBUTTON: return "MMB";
		case VK_XBUTTON1: return "M4";
		case VK_XBUTTON2: return "M5";
		case VK_BACK: return "Backspace";
		case VK_TAB: return "Tab";
		case VK_RETURN: return "Enter";
		case VK_SHIFT: return "Shift";
		case VK_CONTROL: return "Ctrl";
		case VK_MENU: return "Alt";
		case VK_CAPITAL: return "Caps";
		case VK_ESCAPE: return "Esc";
		case VK_SPACE: return "Space";
		case VK_PRIOR: return "PgUp";
		case VK_NEXT: return "PgDn";
		case VK_END: return "End";
		case VK_HOME: return "Home";
		case VK_LEFT: return "Left";
		case VK_UP: return "Up";
		case VK_RIGHT: return "Right";
		case VK_DOWN: return "Down";
		case VK_INSERT: return "Ins";
		case VK_DELETE: return "Del";
		default:
			if (key >= '0' && key <= '9') return std::string(1, (char)key);
			if (key >= 'A' && key <= 'Z') return std::string(1, (char)key);
			if (key >= VK_F1 && key <= VK_F12) return "F" + std::to_string(key - VK_F1 + 1);
			if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) return "Num" + std::to_string(key - VK_NUMPAD0);
			return "Key" + std::to_string(key);
		}
	}
	
	void Interface::RenderKeybindList()
	{
		using namespace ImGui;
		
		// Don't render if menu is open OR if disabled
		if (bIsMenuOpen || !g_Options.General.ShowKeybindList)
			return;
		
		// Update keybind states
		std::vector<KeybindState> newKeybindStates;
		
		// Aimbot
		if (g_Options.LegitBot.AimBot.Enabled && g_Options.LegitBot.AimBot.KeyBind > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.LegitBot.AimBot.KeyBind) & 0x8000) != 0;
			newKeybindStates.push_back({"Aimbot", GetKeyName(g_Options.LegitBot.AimBot.KeyBind), active, 0.f});
		}
		
		// Silent Aim
		if (g_Options.LegitBot.SilentAim.Enabled && g_Options.LegitBot.SilentAim.KeyBind > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.LegitBot.SilentAim.KeyBind) & 0x8000) != 0;
			newKeybindStates.push_back({"Silent", GetKeyName(g_Options.LegitBot.SilentAim.KeyBind), active, 0.f});
		}
		
		// Trigger
		if (g_Options.LegitBot.Trigger.Enabled && g_Options.LegitBot.Trigger.KeyBind > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.LegitBot.Trigger.KeyBind) & 0x8000) != 0;
			newKeybindStates.push_back({"Trigger", GetKeyName(g_Options.LegitBot.Trigger.KeyBind), active, 0.f});
		}
		
		// Magic Bullets
		if (g_Options.LegitBot.MagicBullets.Enabled && g_Options.LegitBot.MagicBullets.KeyBind > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.LegitBot.MagicBullets.KeyBind) & 0x8000) != 0;
			newKeybindStates.push_back({"Magic", GetKeyName(g_Options.LegitBot.MagicBullets.KeyBind), active, 0.f});
		}
		
		// NoClip
		if (g_Options.Exploits.Self.NoClip && g_Options.Exploits.Self.NoClipKey > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.Exploits.Self.NoClipKey) & 0x8000) != 0;
			newKeybindStates.push_back({"NoClip", GetKeyName(g_Options.Exploits.Self.NoClipKey), active, 0.f});
		}
		
		// GodMode
		if (g_Options.Exploits.Self.GodMode && g_Options.Exploits.Self.GodKey > 0)
		{
			bool active = (GetAsyncKeyState(g_Options.Exploits.Self.GodKey) & 0x8000) != 0;
			newKeybindStates.push_back({"GodMode", GetKeyName(g_Options.Exploits.Self.GodKey), active, 0.f});
		}
		
		// Player ESP Toggle
		if (g_Options.Visuals.Players.Toogle && g_Options.Visuals.Players.ToggleKey > 0)
		{
			bool active = g_Options.Visuals.Players.Enabled;
			newKeybindStates.push_back({"Player ESP", GetKeyName(g_Options.Visuals.Players.ToggleKey), active, 0.f});
		}
		
		// Vehicle ESP Toggle
		if (g_Options.Visuals.Vehicles.Toogle && g_Options.Visuals.Vehicles.ToggleKey > 0)
		{
			bool active = g_Options.Visuals.Vehicles.Enabled;
			newKeybindStates.push_back({"Vehicle ESP", GetKeyName(g_Options.Visuals.Vehicles.ToggleKey), active, 0.f});
		}
		
		// Update keybind states with smooth transitions
		keybindStates = newKeybindStates;
		
		// Animate window height
		static float targetHeight = 0.f;
		static float currentHeight = 0.f;
		
		float headerHeight = 45.f;
		float itemHeight = 32.f;
		float padding = 16.f;
		
		targetHeight = keybindStates.empty() ? 0.f : (headerHeight + keybindStates.size() * itemHeight + padding);
		currentHeight = ImLerp(currentHeight, targetHeight, GetIO().DeltaTime * 12.f);
		
		// Don't render if height is too small
		if (currentHeight < 5.f)
			return;
		
		// Calculate window size
		float windowWidth = 220.f;
		
		// Position at top-left
		SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
		SetNextWindowSize(ImVec2(windowWidth, currentHeight), ImGuiCond_Always);
		
		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
		PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
		PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.95f));
		PushStyleColor(ImGuiCol_Border, ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
		
		Begin("##keybinds", nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse);
		{
			ImDrawList* dl = GetWindowDrawList();
			ImVec2 wpos = GetWindowPos();
			ImVec2 wsz = GetWindowSize();
			
			// Header background with gradient
			dl->AddRectFilled(wpos, ImVec2(wpos.x + wsz.x, wpos.y + headerHeight), 
				ImColor(0.08f, 0.08f, 0.08f, 1.f), 6.f, ImDrawFlags_RoundCornersTop);
			
			// Header separator line
			dl->AddLine(ImVec2(wpos.x, wpos.y + headerHeight), 
				ImVec2(wpos.x + wsz.x, wpos.y + headerHeight), 
				ImColor(0.2f, 0.2f, 0.2f, 0.5f), 1.f);
			
			// Title
			SetCursorPos(ImVec2(14, 14));
			PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 0.95f));
			PushFont(ImGui::GetFont()); // Use default font with bold if available
			Text("Keybinds");
			PopFont();
			PopStyleColor();
			
			// Keybind count badge
			if (!keybindStates.empty())
			{
				char countText[8];
				sprintf_s(countText, "%d", (int)keybindStates.size());
				ImVec2 countSize = CalcTextSize(countText);
				ImVec2 badgePos = ImVec2(wpos.x + wsz.x - countSize.x - 20, wpos.y + 14);
				ImVec2 badgeSize = ImVec2(countSize.x + 12, countSize.y + 6);
				
				dl->AddRectFilled(badgePos - ImVec2(6, 3), badgePos + ImVec2(countSize.x + 6, countSize.y + 3),
					ImColor(0.2f, 0.6f, 1.f, 0.3f), 10.f);
				
				SetCursorScreenPos(badgePos);
				PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.f, 0.9f));
				Text("%s", countText);
				PopStyleColor();
			}
			
			// Content area
			SetCursorPos(ImVec2(0, headerHeight));
			BeginChild("##keybinds_content", ImVec2(0, 0), false, ImGuiWindowFlags_NoScrollbar);
			{
				// Render keybinds with animations
				static std::map<std::string, float> activeAnimations;
				static std::map<std::string, float> heightAnimations;
				
				float yOffset = 8.f;
				
				for (size_t i = 0; i < keybindStates.size(); i++)
				{
					auto& kb = keybindStates[i];
					
					// Initialize animations
					if (activeAnimations.find(kb.name) == activeAnimations.end())
						activeAnimations[kb.name] = 0.f;
					if (heightAnimations.find(kb.name) == heightAnimations.end())
						heightAnimations[kb.name] = 0.f;
					
					// Update animations
					float& activeAnim = activeAnimations[kb.name];
					float& heightAnim = heightAnimations[kb.name];
					
					activeAnim = ImLerp(activeAnim, kb.active ? 1.f : 0.f, GetIO().DeltaTime * 10.f);
					heightAnim = ImLerp(heightAnim, 1.f, GetIO().DeltaTime * 15.f);
					
					// Skip if not visible yet
					if (heightAnim < 0.01f)
						continue;
					
					float currentItemHeight = itemHeight * heightAnim;
					
					ImVec2 itemPos = GetCursorScreenPos();
					ImVec2 itemSize = ImVec2(windowWidth, currentItemHeight);
					
					// Hover effect
					bool hovered = IsMouseHoveringRect(itemPos, itemPos + itemSize);
					static std::map<std::string, float> hoverAnimations;
					if (hoverAnimations.find(kb.name) == hoverAnimations.end())
						hoverAnimations[kb.name] = 0.f;
					
					float& hoverAnim = hoverAnimations[kb.name];
					hoverAnim = ImLerp(hoverAnim, hovered ? 1.f : 0.f, GetIO().DeltaTime * 12.f);
					
					// Background on hover
					if (hoverAnim > 0.01f)
					{
						dl->AddRectFilled(itemPos, itemPos + itemSize,
							ImColor(1.f, 1.f, 1.f, 0.03f * hoverAnim));
					}
					
					// Left accent line when active
					if (activeAnim > 0.01f)
					{
						dl->AddRectFilled(itemPos, ImVec2(itemPos.x + 3, itemPos.y + currentItemHeight),
							ImColor(0.3f, 1.f, 0.3f, activeAnim * 0.8f));
					}
					
					// Text content
					SetCursorScreenPos(ImVec2(itemPos.x + 14, itemPos.y + (currentItemHeight - CalcTextSize("A").y) * 0.5f));
					
					PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 0.85f * heightAnim));
					Text("%s", kb.name.c_str());
					PopStyleColor();
					
					// Key badge
					ImVec2 keyTextSize = CalcTextSize(kb.key.c_str());
					ImVec2 keyBadgePos = ImVec2(itemPos.x + windowWidth - keyTextSize.x - 40, itemPos.y + (currentItemHeight - keyTextSize.y - 6) * 0.5f);
					ImVec2 keyBadgeSize = ImVec2(keyTextSize.x + 12, keyTextSize.y + 6);
					
					dl->AddRectFilled(keyBadgePos, keyBadgePos + keyBadgeSize,
						ImColor(0.12f, 0.12f, 0.12f, heightAnim), 4.f);
					dl->AddRect(keyBadgePos, keyBadgePos + keyBadgeSize,
						ImColor(0.25f, 0.25f, 0.25f, 0.5f * heightAnim), 4.f, 0, 1.f);
					
					SetCursorScreenPos(ImVec2(keyBadgePos.x + 6, keyBadgePos.y + 3));
					PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 0.8f * heightAnim));
					Text("%s", kb.key.c_str());
					PopStyleColor();
					
					// Active indicator (circle with glow)
					if (activeAnim > 0.01f)
					{
						ImVec2 circlePos = ImVec2(itemPos.x + windowWidth - 16, itemPos.y + currentItemHeight * 0.5f);
						float circleRadius = 4.f;
						
						// Outer glow
						dl->AddCircleFilled(circlePos, circleRadius + 3.f, 
							ImColor(0.3f, 1.f, 0.3f, activeAnim * 0.2f), 16);
						
						// Middle glow
						dl->AddCircleFilled(circlePos, circleRadius + 1.5f, 
							ImColor(0.3f, 1.f, 0.3f, activeAnim * 0.4f), 16);
						
						// Main circle
						dl->AddCircleFilled(circlePos, circleRadius, 
							ImColor(0.3f, 1.f, 0.3f, activeAnim), 16);
					}
					
					// Bottom separator line (except last item)
					if (i < keybindStates.size() - 1)
					{
						dl->AddLine(ImVec2(itemPos.x + 14, itemPos.y + currentItemHeight),
							ImVec2(itemPos.x + windowWidth - 14, itemPos.y + currentItemHeight),
							ImColor(0.15f, 0.15f, 0.15f, 0.3f * heightAnim), 1.f);
					}
					
					SetCursorScreenPos(ImVec2(itemPos.x, itemPos.y + currentItemHeight));
					yOffset += currentItemHeight;
				}
				
				// Clean up animations for removed keybinds
				std::vector<std::string> toRemove;
				for (auto& [name, anim] : heightAnimations)
				{
					bool found = false;
					for (auto& kb : keybindStates)
					{
						if (kb.name == name)
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						heightAnimations[name] = ImLerp(heightAnimations[name], 0.f, GetIO().DeltaTime * 15.f);
						if (heightAnimations[name] < 0.01f)
							toRemove.push_back(name);
					}
				}
				for (auto& name : toRemove)
				{
					heightAnimations.erase(name);
					activeAnimations.erase(name);
				}
			}
			EndChild();
		}
		End();
		
		PopStyleColor(2);
		PopStyleVar(3);
	}

	void Interface::RenderWatermark()
	{
		if (!g_Options.General.ShowWatermark)
			return;
		/*
		using namespace ImGui;
		
		std::string watermarkText = "Mello External";
		
		ImVec2 textSize = CalcTextSize(watermarkText.c_str());
		ImVec2 pos = ImVec2(GetIO().DisplaySize.x - textSize.x - 20, 10);
		
		ImDrawList* dl = GetForegroundDrawList();
		
		// Background
		dl->AddRectFilled(
			ImVec2(pos.x - 10, pos.y - 5),
			ImVec2(pos.x + textSize.x + 10, pos.y + textSize.y + 5),
			ImColor(0, 0, 0, 180),
			4.f
		);
		
		// Border
		dl->AddRect(
			ImVec2(pos.x - 10, pos.y - 5),
			ImVec2(pos.x + textSize.x + 10, pos.y + textSize.y + 5),
			ImColor(20, 20, 20, 200),
			4.f,
			0,
			1.5f
		);
		
		// Text shadow
		dl->AddText(ImVec2(pos.x + 1, pos.y + 1), ImColor(0, 0, 0, 255), watermarkText.c_str());
		// Text
		dl->AddText(pos, ImColor(255, 255, 255, 255), watermarkText.c_str());
		*/
	}

	void Interface::RenderSessionInfo()
	{
		if (!g_Options.Exploits.Session.ShowSessionInfo)
			return;

		using namespace ImGui;
		int currentTime = (int)time(0);
		int sessionTime = currentTime - g_Options.Exploits.Session.SessionStartTime;
		int hours = sessionTime / 3600;
		int minutes = (sessionTime % 3600) / 60;
		int seconds = sessionTime % 60;
		
		char timeStr[64];
		sprintf_s(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
		
		ImVec2 textSize = CalcTextSize(timeStr);
		ImVec2 pos = ImVec2(10, GetIO().DisplaySize.y - textSize.y - 20);
		
		ImDrawList* dl = GetForegroundDrawList();
		
		// Background
		dl->AddRectFilled(
			ImVec2(pos.x - 10, pos.y - 5),
			ImVec2(pos.x + textSize.x + 10, pos.y + textSize.y + 5),
			ImColor(0, 0, 0, 180),
			4.f
		);
		
		// Border
		dl->AddRect(
			ImVec2(pos.x - 10, pos.y - 5),
			ImVec2(pos.x + textSize.x + 10, pos.y + textSize.y + 5),
			ImColor(100, 255, 100, 200),
			4.f,
			0,
			1.5f
		);
		
		// Text shadow
		dl->AddText(ImVec2(pos.x + 1, pos.y + 1), ImColor(0, 0, 0, 255), timeStr);
		// Text
		dl->AddText(pos, ImColor(255, 255, 255, 255), timeStr);
	}

	void Interface::AddConfigNotification(const std::string& message)
	{
		ConfigNotification notif;
		notif.message = message;
		notif.lifeTime = 0.f;
		notif.fadeAnim = 0.f;
		configNotifications.push_back(notif);
	}

	void Interface::RenderConfigNotifications()
	{
		using namespace ImGui;
		
		for (size_t i = 0; i < configNotifications.size(); )
		{
			ConfigNotification& notif = configNotifications[i];
			notif.lifeTime += GetIO().DeltaTime;
			float totalDuration = 3.f;
			if (notif.lifeTime < 0.3f)
			{
				float target = 1.f;
				notif.fadeAnim = notif.fadeAnim + (target - notif.fadeAnim) * GetIO().DeltaTime * 10.f;
			}
			else if (notif.lifeTime > totalDuration - 0.5f)
			{
				float target = 0.f;
				notif.fadeAnim = notif.fadeAnim + (target - notif.fadeAnim) * GetIO().DeltaTime * 8.f;
			}
			else
			{
				notif.fadeAnim = 1.f;
			}
			
			// Remove if done
			if (notif.lifeTime > totalDuration && notif.fadeAnim < 0.01f)
			{
				configNotifications.erase(configNotifications.begin() + i);
				continue;
			}
			
			// Calculate alpha
			int alpha = (int)(notif.fadeAnim * 255.f);
			
			// Center position
			ImVec2 textSize = CalcTextSize(notif.message.c_str());
			ImVec2 pos = ImVec2(
				(GetIO().DisplaySize.x - textSize.x) * 0.5f,
				GetIO().DisplaySize.y * 0.85f
			);
			
			// Draw text with shadow
			ImDrawList* dl = GetForegroundDrawList();
			dl->AddText(ImVec2(pos.x + 2, pos.y + 2), ImColor(0, 0, 0, alpha), notif.message.c_str());
			dl->AddText(pos, ImColor(100, 255, 100, alpha), notif.message.c_str());
			
			i++;
		}
	}
}
