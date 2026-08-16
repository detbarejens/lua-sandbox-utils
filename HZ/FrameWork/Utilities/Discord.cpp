#include "Discord.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include "../../Definations/Variables.hpp"

namespace FrameWork
{
	namespace Discord
	{
		void GetDcName()
		{
			char appDataPath[MAX_PATH];
			std::string discordUsername;

			if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath)))
				return;

			std::vector<std::string> possibleFolders = {
				"\\discord\\Local Storage\\leveldb\\",
				"\\discordcanary\\Local Storage\\leveldb\\",
				"\\discordptb\\Local Storage\\leveldb\\"
			};

			for (const auto& subFolder : possibleFolders)
			{
				std::string discordPath = std::string(appDataPath) + subFolder;
				std::string pattern = discordPath + "*.ldb";

				WIN32_FIND_DATAA findData;
				HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);

				if (hFind == INVALID_HANDLE_VALUE)
					continue;

				do
				{
					std::string filePath = discordPath + findData.cFileName;
					std::ifstream file(filePath, std::ios::binary);
					if (!file.good())
						continue;

					std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
					file.close();


					std::vector<std::string> possibleDomains = {
						"discord.com",
						"discordapp.com",
						"canary.discord.com",
						"ptb.discord.com"
					};

					bool found = false;
					for (auto& domain : possibleDomains)
					{
						size_t pos = 0;
						while ((pos = content.find(domain, pos)) != std::string::npos)
						{
							const int scanAhead = 100;
							size_t endPos = (std::min)(pos + domain.size() + scanAhead, content.size());
							std::string chunk = content.substr(pos, endPos - pos);

							std::regex re("\\b(\\d{17,19})\\b");
							std::smatch match;
							if (std::regex_search(chunk, match, re))
							{
								found = true;
								break;
							}
							pos += domain.size();
						}
						if (found) break;
					}

					if (found)
					{
						std::regex usernameRe("\"username\"\\s*:\\s*\"([^\"]+)\"");
						std::smatch usernameMatch;
						if (std::regex_search(content, usernameMatch, usernameRe))
						{
							discordUsername = usernameMatch.str(1);
						}
					}

					if (!discordUsername.empty())
						break;

				} while (FindNextFileA(hFind, &findData));

				FindClose(hFind);

				if (!discordUsername.empty())
					break;
			}

		
			if (!discordUsername.empty())
				g_Options.General.DiscordUsername = discordUsername;
		}
	}
}
