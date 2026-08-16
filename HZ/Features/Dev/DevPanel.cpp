#include "DevPanel.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "DevBridge.hpp"
#include "LuaRuntime.hpp"
#include "ServerDump.hpp"
#include "../../Utils/BrandPaths.hpp"
#include "../../WebControl/WebServer.hpp"
#include "../../Render/CustomWidgets/Notify.hpp"
#include "../../Slate/include/slate.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Cheat
{
	namespace DevPanel
	{
		namespace
		{
			char g_codeBuffer[65536] = "-- Click here to edit your code\n";
			char g_blockEventBuffer[128] = {};
			char g_dumpDirectory[260] = {};
			char g_resourceSearch[128] = {};
			char g_triggerSearch[128] = {};
			char g_eventSearch[128] = {};
			bool g_autoRefreshEvents = true;
			bool g_dumpStreamables = true;
			bool g_dumpScripts = true;
			bool g_dumpAllFiles = false;
			int g_selectedResource = -1;
			int g_selectedLuaFile = -1;
			std::vector<std::string> g_luaFiles;

			void EnsureLuaDirectory()
			{
				const std::string luaDir = BrandPaths::GetDataRoot() + "lua";
				std::error_code ec;
				std::filesystem::create_directories(luaDir, ec);
			}

			void RefreshLuaFileList()
			{
				g_luaFiles.clear();
				EnsureLuaDirectory();

				const std::string luaDir = BrandPaths::GetDataRoot() + "lua";
				std::error_code ec;
				for (const auto& entry : std::filesystem::directory_iterator(luaDir, ec))
				{
					if (!entry.is_regular_file())
						continue;

					if (entry.path().extension() == ".lua")
						g_luaFiles.push_back(entry.path().filename().string());
				}
			}

			std::string ReadLuaFile(const std::string& filename)
			{
				const std::string path = BrandPaths::GetDataRoot() + "lua\\" + filename;
				std::ifstream file(path, std::ios::binary);
				if (!file)
					return {};

				std::ostringstream ss;
				ss << file.rdbuf();
				return ss.str();
			}

			void DrawStandaloneStatus()
			{
				const auto state = DevBridge::GetState();
				if (DevBridge::IsFiveMAttached())
					ImGui::TextColored(ImVec4(0.35f, 0.95f, 0.55f, 1.f), "Attached — dumping client scripts from FiveM memory");
				else
					ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.35f, 1.f), "Waiting for FiveM — join a server first");
				const auto runtime = LuaRuntime::GetStatus();
				if (DevBridge::IsFiveMAttached())
				{
					if (runtime.harvesting)
						ImGui::TextDisabled("Reading server memory for client scripts...");
					else if (runtime.ready)
						ImGui::TextDisabled("Runtime ready: %zu client scripts from %zu resources",
							runtime.clientScripts, runtime.resourceCount);
					else
						ImGui::TextDisabled("Join server — client.luas harvest automatically (~6s after attach)");
				}
				if (state.connected)
					ImGui::TextDisabled("Optional bridge connected");
				if (!state.serverLabel.empty() && !state.serverCachePath.empty())
				{
					ImGui::TextDisabled("Active server: %s", state.serverLabel.c_str());
					ImGui::TextDisabled("Cache: %s", state.serverCachePath.c_str());
					ImGui::TextDisabled("Cache blobs: %zu | Scripts: %zu | Resources: %zu",
						state.serverStreamFiles, state.serverScriptFiles, state.resources.size());
				}
				ImGui::TextDisabled("%s", DevBridge::GetStatusMessage().c_str());
				ImGui::TextDisabled("Dump folder: %sdump\\", BrandPaths::GetDataRoot().c_str());
				ImGui::TextDisabled("Folder is created when you run Dump Active Server (Lua State tab).");
			}

			void DrawEventTable(const char* id, const std::vector<DevEventEntry>& events, const char* channel, bool allowResend)
			{
				ImGui::InputTextWithHint("##search", "Search events...", g_eventSearch, sizeof(g_eventSearch));

				if (ImGui::BeginTable(id, 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 260)))
				{
					ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 110.f);
					ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 80.f);
					ImGui::TableHeadersRow();

					const std::string filter = g_eventSearch;
					for (size_t i = 0; i < events.size(); ++i)
					{
						const auto& entry = events[i];
						if (!filter.empty())
						{
							const std::string haystack = entry.event + entry.data;
							if (haystack.find(filter) == std::string::npos)
								continue;
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(entry.timestamp.c_str());
						ImGui::TableSetColumnIndex(1);
						ImGui::TextWrapped("%s %s", entry.event.c_str(), entry.data.c_str());
						ImGui::TableSetColumnIndex(2);
						ImGui::PushID(static_cast<int>(i));
						if (allowResend && ImGui::SmallButton("Resend"))
						{
							if (std::string(channel) == "cts")
								DevBridge::ResendCtsEvent(i);
							else
								DevBridge::ResendStcEvent(i);
						}
						ImGui::PopID();
					}

					ImGui::EndTable();
				}
			}
		}

		void RegisterPages(int tabIndex)
		{
			if (g_dumpDirectory[0] == '\0')
			{
				const std::string dumpRoot = BrandPaths::GetDataRoot() + "dump";
				strncpy_s(g_dumpDirectory, dumpRoot.c_str(), _TRUNCATE);
			}

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				widgets->child.begin("Develop", 9001);
				{
					ImGui::InputTextMultiline("##dev_code", g_codeBuffer, sizeof(g_codeBuffer), ImVec2(-1.f, 360.f));
					if (widgets->button("Run", ImVec2(-1.f, 32.f)))
						DevBridge::ExecuteLua(g_codeBuffer);
				}
				widgets->child.end();
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				if (ImGui::Button("Refresh list"))
					RefreshLuaFileList();

				widgets->child.begin("Load Lua Files", 9002);
				{
					if (g_luaFiles.empty())
						RefreshLuaFileList();

					if (ImGui::BeginTable("lua_files", 1, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 320)))
					{
						ImGui::TableSetupColumn("Script Name");
						ImGui::TableHeadersRow();

						for (int i = 0; i < static_cast<int>(g_luaFiles.size()); ++i)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							const bool selected = g_selectedLuaFile == i;
							if (ImGui::Selectable(g_luaFiles[i].c_str(), selected))
								g_selectedLuaFile = i;
						}

						ImGui::EndTable();
					}

					ImGui::TextDisabled("Place .lua files in: %slua\\", BrandPaths::GetDataRoot().c_str());
					if (widgets->button("Open", ImVec2(-1.f, 30.f)) && g_selectedLuaFile >= 0 && g_selectedLuaFile < static_cast<int>(g_luaFiles.size()))
					{
						const std::string code = ReadLuaFile(g_luaFiles[g_selectedLuaFile]);
						if (!code.empty())
						{
							strncpy_s(g_codeBuffer, code.c_str(), _TRUNCATE);
							DevBridge::ExecuteLua(code);
						}
					}
				}
				widgets->child.end();
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				const auto state = DevBridge::GetState();

				ImGui::BeginGroup();
				{
					widgets->child.begin("Lua Threads", 9003);
					{
						if (ImGui::BeginTable("lua_threads", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 180)))
						{
							ImGui::TableSetupColumn("Script Name");
							ImGui::TableSetupColumn("Status");
							ImGui::TableSetupColumn("-");
							ImGui::TableHeadersRow();
							for (const auto& thread : state.threads)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(thread.name.c_str());
								ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(thread.status.c_str());
								ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("-");
							}
							ImGui::EndTable();
						}
					}
					widgets->child.end();

					widgets->child.begin("Lua Reset", 9004);
					{
						if (widgets->button("Reset Environment", ImVec2(-1.f, 30.f)))
							DevBridge::ResetLuaEnvironment();
					}
					widgets->child.end();
				}
				ImGui::EndGroup();

				ImGui::SameLine();

				ImGui::BeginGroup();
				{
					widgets->child.begin("Dumper", 9005);
					{
						if (ImGui::Button("Scan Server"))
							DevBridge::ScanServer();

						ImGui::InputText("Directory", g_dumpDirectory, sizeof(g_dumpDirectory));
						ImGui::Checkbox("Include stream files (.yft, .ytd, .rpf, stream/)", &g_dumpStreamables);
						ImGui::Checkbox("Include scripts (.lua, .js, .html)", &g_dumpScripts);
						ImGui::Checkbox("Include all cached files", &g_dumpAllFiles);
						if (widgets->button("Dump Active Server", ImVec2(-1.f, 30.f)))
						{
							DevBridge::DumpActiveServer(
								g_dumpDirectory,
								g_dumpStreamables,
								g_dumpScripts,
								g_dumpAllFiles);
						}

						const ServerDump::DumpProgressState dumpProgress = ServerDump::GetDumpProgress();
						if (dumpProgress.active)
						{
							char progressLabel[192];
							if (!dumpProgress.detail.empty())
								snprintf(progressLabel, sizeof(progressLabel), "%s - %s", dumpProgress.phase.c_str(), dumpProgress.detail.c_str());
							else
								snprintf(progressLabel, sizeof(progressLabel), "%s", dumpProgress.phase.c_str());

							ImGui::ProgressBar(dumpProgress.progress, ImVec2(-1.f, 18.f), progressLabel);
						}
						else if (dumpProgress.progress >= 1.f && !dumpProgress.phase.empty() && dumpProgress.phase != "Idle")
						{
							ImGui::ProgressBar(1.f, ImVec2(-1.f, 18.f), "Dump complete");
						}
						ImGui::TextDisabled("Output: %s<server>\\<resource>\\fxmanifest.lua + all client/shared/nui files", g_dumpDirectory);
						ImGui::TextDisabled("Always scans live FiveM memory for client-side files (not just cache).");
					}
					widgets->child.end();

					widgets->child.begin("Event Blocker", 9006);
					{
						ImGui::InputTextWithHint("##block_event", "e.g BanPlayer", g_blockEventBuffer, sizeof(g_blockEventBuffer));
						if (widgets->button("Block", ImVec2(-1.f, 30.f)) && g_blockEventBuffer[0])
						{
							DevBridge::BlockEvent(g_blockEventBuffer);
							g_blockEventBuffer[0] = '\0';
						}

						if (ImGui::BeginTable("blocked_events", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 120)))
						{
							ImGui::TableSetupColumn("Event Name");
							ImGui::TableSetupColumn("Block Count");
							ImGui::TableHeadersRow();
							for (const auto& blocked : state.blockedEvents)
							{
								ImGui::TableNextRow();
								ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(blocked.c_str());
								ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted("1");
							}
							ImGui::EndTable();
						}
					}
					widgets->child.end();
				}
				ImGui::EndGroup();
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				const auto state = DevBridge::GetState();
				const std::string authKey = WebControl::GenerateHWID();

				widgets->child.begin("Lua Settings", 9007);
				{
					bool logCts = state.logCts;
					bool logStc = state.logStc;
					bool logNui = state.logNui;

					if (widgets->checkbox("Log Server Events (CTS)", &logCts))
						DevBridge::SetSetting("logCts", logCts);
					if (widgets->checkbox("Log Client Events (STC)", &logStc))
						DevBridge::SetSetting("logStc", logStc);
					if (widgets->checkbox("Log NUI Messages", &logNui))
						DevBridge::SetSetting("logNui", logNui);
				}
				widgets->child.end();

				widgets->child.begin("Other Settings", 9008);
				{
					if (widgets->button("Test JS/NUI Injection", ImVec2(-1.f, 30.f)))
						DevBridge::TestNuiInject();
				}
				widgets->child.end();

				widgets->child.begin("Authentication Key", 9009);
				{
					ImGui::TextWrapped("%s", authKey.c_str());
					ImGui::TextDisabled("Share this with a developer to gain access to their menu (left click to copy).");
					if (ImGui::IsItemClicked(0))
					{
						if (OpenClipboard(nullptr))
						{
							EmptyClipboard();
							const size_t size = authKey.size() + 1;
							HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
							if (memory)
							{
								memcpy(GlobalLock(memory), authKey.c_str(), size);
								GlobalUnlock(memory);
								SetClipboardData(CF_TEXT, memory);
							}
							CloseClipboard();
						}
					}
				}
				widgets->child.end();
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				ImGui::InputTextWithHint("##trigger_search", "Search triggers...", g_triggerSearch, sizeof(g_triggerSearch));
				if (ImGui::Button("Scan Server"))
					DevBridge::ScanServer();
				ImGui::SameLine();
				ImGui::TextDisabled("Scans decrypted cache + FiveM memory while attached. Raw dump files are encrypted.");

				const auto state = DevBridge::GetState();
				if (ImGui::BeginTable("triggers", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 360)))
				{
					ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthFixed, 140.f);
					ImGui::TableSetupColumn("Parsed Data", ImGuiTableColumnFlags_WidthStretch);
					ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 140.f);
					ImGui::TableHeadersRow();

					const std::string filter = g_triggerSearch;
					for (size_t i = 0; i < state.triggers.size(); ++i)
					{
						const auto& trigger = state.triggers[i];
						if (!filter.empty())
						{
							const std::string haystack = trigger.resource + trigger.code;
							if (haystack.find(filter) == std::string::npos)
								continue;
						}

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TextUnformatted(trigger.resource.c_str());
						ImGui::TableSetColumnIndex(1);
						ImGui::TextColored(ImVec4(0.45f, 0.65f, 1.f, 1.f), "%s", trigger.code.c_str());
						ImGui::TableSetColumnIndex(2);
						ImGui::PushID(static_cast<int>(i));
						if (ImGui::SmallButton("Execute"))
							DevBridge::ExecuteLua(trigger.code);
						ImGui::SameLine();
						if (ImGui::SmallButton("Copy"))
							strncpy_s(g_codeBuffer, trigger.code.c_str(), _TRUNCATE);
						ImGui::PopID();
					}

					ImGui::EndTable();
				}
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				ImGui::Checkbox("Auto-Refresh", &g_autoRefreshEvents);
				ImGui::SameLine();
				if (ImGui::Button("Refresh")) {}
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
					DevBridge::ClearEvents("cts");
				ImGui::SameLine();
				if (ImGui::Button("Export"))
				{
					const auto state = DevBridge::GetState();
					const std::string path = BrandPaths::GetDataRoot() + "logs\\cts_export.txt";
					std::error_code ec;
					std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
					std::ofstream out(path, std::ios::trunc);
					for (const auto& entry : state.ctsEvents)
						out << entry.timestamp << '\t' << entry.event << '\t' << entry.data << '\n';
				}

				const auto state = DevBridge::GetState();
				ImGui::TextDisabled("Live event capture needs an in-game executor. Enable logging and use Find Triggers > Execute to log copied trigger lines.");
				DrawEventTable("cts_events", state.ctsEvents, "cts", true);
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				ImGui::Checkbox("Auto-Refresh", &g_autoRefreshEvents);
				ImGui::SameLine();
				if (ImGui::Button("Refresh")) {}
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
					DevBridge::ClearEvents("stc");
				ImGui::SameLine();
				if (ImGui::Button("Export"))
				{
					const auto state = DevBridge::GetState();
					const std::string path = BrandPaths::GetDataRoot() + "logs\\stc_export.txt";
					std::error_code ec;
					std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
					std::ofstream out(path, std::ios::trunc);
					for (const auto& entry : state.stcEvents)
						out << entry.timestamp << '\t' << entry.event << '\t' << entry.data << '\n';
				}

				const auto state = DevBridge::GetState();
				ImGui::TextDisabled("Live STC capture is not available from external. Copied trigger lines can be logged when Log Client Events is enabled.");
				DrawEventTable("stc_events", state.stcEvents, "stc", true);
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				ImGui::InputTextWithHint("##resource_search", "Search resources...", g_resourceSearch, sizeof(g_resourceSearch));
				if (ImGui::Button("Refresh Server"))
					DevBridge::ScanServer();
				ImGui::SameLine();
				ImGui::TextDisabled("Active server cache only");
				const auto state = DevBridge::GetState();

				if (ImGui::BeginTable("resources", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 360)))
				{
					ImGui::TableSetupColumn("Resource");
					ImGui::TableSetupColumn("State");
					ImGui::TableSetupColumn("Thread Count");
					ImGui::TableSetupColumn("Event Count");
					ImGui::TableHeadersRow();

					const std::string filter = g_resourceSearch;
					for (int i = 0; i < static_cast<int>(state.resources.size()); ++i)
					{
						const auto& resource = state.resources[i];
						if (!filter.empty() && resource.name.find(filter) == std::string::npos)
							continue;

						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						if (ImGui::Selectable(resource.name.c_str(), g_selectedResource == i, ImGuiSelectableFlags_SpanAllColumns))
							g_selectedResource = i;
						ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(resource.state.c_str());
						ImGui::TableSetColumnIndex(2); ImGui::Text("%d", resource.threadCount);
						ImGui::TableSetColumnIndex(3); ImGui::Text("%d", resource.eventCount);
					}

					ImGui::EndTable();
				}

				if (g_selectedResource >= 0 && g_selectedResource < static_cast<int>(state.resources.size()))
				{
					const std::string& name = state.resources[g_selectedResource].name;
					if (ImGui::Button("Start Resource"))
						DevBridge::StartResource(name);
					ImGui::SameLine();
					if (ImGui::Button("Stop Resource"))
						DevBridge::StopResource(name);
					ImGui::TextDisabled("Copies ensure/stop command to clipboard (F8/server console).");
				}
			});

			widgets->nav.addpage(tabIndex, [&]()
			{
				DrawStandaloneStatus();
				ImGui::Checkbox("Auto-Refresh", &g_autoRefreshEvents);
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
					DevBridge::ClearEvents("nui");
				ImGui::SameLine();
				if (ImGui::Button("Export"))
				{
					const auto state = DevBridge::GetState();
					const std::string path = BrandPaths::GetDataRoot() + "logs\\nui_export.txt";
					std::error_code ec;
					std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
					std::ofstream out(path, std::ios::trunc);
					for (const auto& entry : state.nuiEvents)
						out << entry.timestamp << '\t' << entry.event << '\t' << entry.data << '\n';
				}

				const auto state = DevBridge::GetState();
				DrawEventTable("nui_events", state.nuiEvents, "nui", false);
			});
		}
	}
}

#endif
