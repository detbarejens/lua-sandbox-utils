#include "LuaProcessInject.hpp"

#if defined(TRINITY_DEV) && TRINITY_DEV

#include "../../Utils/Memory.hpp"

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <mutex>
#include <string>
#include <vector>

#ifndef QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC
#define QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC 0x00000001
#endif

namespace Cheat
{
	namespace LuaProcessInject
	{
		namespace
		{
			std::mutex g_mutex;
			std::string g_lastError;
			uintptr_t g_luaModuleBase = 0;
			uintptr_t g_luaModuleEnd = 0;
			uintptr_t g_luaLoad = 0;
			uintptr_t g_luaPcall = 0;

			struct Region
			{
				uintptr_t start = 0;
				uintptr_t end = 0;
			};

			void SetError(const char* message)
			{
				g_lastError = message ? message : "unknown error";
			}

			template <typename T>
			T ReadRemote(uintptr_t address)
			{
				T value{};
				SIZE_T read = 0;
				ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<LPCVOID>(address), &value, sizeof(T), &read);
				return value;
			}

			bool InRange(uintptr_t address, uintptr_t start, uintptr_t end)
			{
				return address >= start && address < end;
			}

			std::vector<Region> EnumerateHeapRegions()
			{
				std::vector<Region> regions;
				SYSTEM_INFO systemInfo{};
				GetSystemInfo(&systemInfo);

				for (uint64_t address = reinterpret_cast<uint64_t>(systemInfo.lpMinimumApplicationAddress);
					address < reinterpret_cast<uint64_t>(systemInfo.lpMaximumApplicationAddress);)
				{
					MEMORY_BASIC_INFORMATION info{};
					if (!VirtualQueryEx(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<LPCVOID>(address), &info, sizeof(info)))
					{
						address += systemInfo.dwPageSize;
						continue;
					}

					const uint64_t base = reinterpret_cast<uint64_t>(info.BaseAddress);
					const uint64_t size = static_cast<uint64_t>(info.RegionSize);
					if (info.State == MEM_COMMIT &&
						(info.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) &&
						!(info.Protect & PAGE_GUARD) &&
						(info.Type == MEM_PRIVATE || info.Type == MEM_MAPPED) &&
						size >= 0x1000 && size <= 64ull * 1024ull * 1024ull)
					{
						regions.push_back({ base, base + size });
					}

					address = base + size;
				}

				return regions;
			}

			bool RegionContains(const std::vector<Region>& regions, uintptr_t address)
			{
				if (address < 0x10000 || address > 0x7FFFFFFFFFFF)
					return false;
				for (const auto& region : regions)
				{
					if (InRange(address, region.start, region.end))
						return true;
				}
				return false;
			}

			bool LooksLikeLuaState(uintptr_t address, const std::vector<Region>& regions)
			{
				if ((address & 7) != 0 || !RegionContains(regions, address) || !RegionContains(regions, address + 32))
					return false;
				if (ReadRemote<uint8_t>(address + 8) != 8)
					return false;
				const uintptr_t globalState = ReadRemote<uintptr_t>(address + 24);
				return globalState != address && (globalState & 7) == 0 && RegionContains(regions, globalState);
			}

			uintptr_t FindStringOffset(const std::vector<uint8_t>& bytes, const char* needle)
			{
				const size_t needleSize = std::strlen(needle);
				if (needleSize == 0 || bytes.size() < needleSize)
					return UINTPTR_MAX;
				for (size_t i = 0; i + needleSize <= bytes.size(); ++i)
				{
					if (std::memcmp(bytes.data() + i, needle, needleSize) == 0)
						return i;
				}
				return UINTPTR_MAX;
			}

			uintptr_t FindPrologue(const std::vector<uint8_t>& bytes, uintptr_t moduleBase, size_t insnOffset)
			{
				size_t index = insnOffset;
				const size_t limit = insnOffset > 384 ? insnOffset - 384 : 0;
				while (index > limit)
				{
					const uint8_t previous = bytes[index - 1];
					if (previous == 0xCC || previous == 0xC3 || previous == 0xC2)
						break;
					--index;
				}
				while (index < insnOffset && bytes[index] == 0xCC)
					++index;
				return moduleBase + index;
			}

			uintptr_t FindXrefFunction(const std::vector<uint8_t>& bytes, uintptr_t moduleBase, const char* needle)
			{
				const uintptr_t stringOffset = FindStringOffset(bytes, needle);
				if (stringOffset == UINTPTR_MAX)
					return 0;

				const uintptr_t stringAddress = moduleBase + stringOffset;
				for (size_t i = 0; i + 7 < bytes.size(); ++i)
				{
					const uint8_t rex = bytes[i];
					if ((rex != 0x48 && rex != 0x4C) || bytes[i + 1] != 0x8D)
						continue;
					if ((bytes[i + 2] & 0xC7) != 0x05)
						continue;
					const int32_t displacement = *reinterpret_cast<const int32_t*>(&bytes[i + 3]);
					if (moduleBase + i + 7 + displacement == stringAddress)
						return FindPrologue(bytes, moduleBase, i);
				}
				return 0;
			}

			uintptr_t FindExport(uintptr_t moduleBase, const std::vector<uint8_t>& image, const char* name)
			{
				if (image.size() < sizeof(IMAGE_DOS_HEADER))
					return 0;
				const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image.data());
				if (dos->e_magic != IMAGE_DOS_SIGNATURE)
					return 0;
				if (static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) > image.size())
					return 0;
				const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image.data() + dos->e_lfanew);
				const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
				if (!dir.VirtualAddress || dir.VirtualAddress + sizeof(IMAGE_EXPORT_DIRECTORY) > image.size())
					return 0;

				const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(image.data() + dir.VirtualAddress);
				if (exports->AddressOfNames + exports->NumberOfNames * 4 > image.size())
					return 0;

				const auto* names = reinterpret_cast<const uint32_t*>(image.data() + exports->AddressOfNames);
				const auto* ords = reinterpret_cast<const uint16_t*>(image.data() + exports->AddressOfNameOrdinals);
				const auto* funcs = reinterpret_cast<const uint32_t*>(image.data() + exports->AddressOfFunctions);

				for (uint32_t i = 0; i < exports->NumberOfNames; ++i)
				{
					if (names[i] >= image.size())
						continue;
					const char* exportName = reinterpret_cast<const char*>(image.data() + names[i]);
					if (std::strcmp(exportName, name) != 0)
						continue;
					const uint16_t ordinal = ords[i];
					if (ordinal >= exports->NumberOfFunctions)
						continue;
					return moduleBase + funcs[ordinal];
				}
				return 0;
			}

			uintptr_t FirstInModuleCall(const std::vector<uint8_t>& bytes, uintptr_t moduleBase, uintptr_t function)
			{
				if (function < moduleBase)
					return 0;
				const size_t start = static_cast<size_t>(function - moduleBase);
				const size_t end = (std::min)(bytes.size() - 5, start + 0x500);
				for (size_t i = start; i < end; ++i)
				{
					if (bytes[i] != 0xE8)
						continue;
					const int32_t rel = *reinterpret_cast<const int32_t*>(&bytes[i + 1]);
					const uintptr_t dest = moduleBase + i + 5 + rel;
					if (dest > moduleBase + 0x1000 && dest < moduleBase + bytes.size() && dest != function)
						return dest;
				}
				return 0;
			}

			HANDLE SnapshotModules()
			{
				for (int attempt = 0; attempt < 10; ++attempt)
				{
					HANDLE snapshot = CreateToolhelp32Snapshot(
						TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, FrameWork::Memory::AttachedProcessPid);
					if (snapshot != INVALID_HANDLE_VALUE && snapshot)
						return snapshot;
					Sleep(25);
				}
				return INVALID_HANDLE_VALUE;
			}

			bool FindLuaModule()
			{
				HANDLE snapshot = SnapshotModules();
				if (snapshot == INVALID_HANDLE_VALUE)
					return false;

				MODULEENTRY32W entry{};
				entry.dwSize = sizeof(entry);
				uintptr_t fallbackBase = 0;
				size_t fallbackSize = 0;

				if (Module32FirstW(snapshot, &entry))
				{
					do
					{
						std::wstring name = entry.szModule;
						for (auto& c : name)
							c = static_cast<wchar_t>(towlower(c));

						const bool exact = name == L"citizen-scripting-lua.dll";
						const bool fuzzy = name.find(L"scripting-lua") != std::wstring::npos ||
							name == L"lua54.dll" || name == L"lua.dll";
						if (!exact && !fuzzy)
							continue;

						if (exact)
						{
							g_luaModuleBase = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
							g_luaModuleEnd = g_luaModuleBase + entry.modBaseSize;
							CloseHandle(snapshot);
							return g_luaModuleBase != 0;
						}

						if (!fallbackBase)
						{
							fallbackBase = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
							fallbackSize = entry.modBaseSize;
						}
					} while (Module32NextW(snapshot, &entry));
				}

				CloseHandle(snapshot);
				if (!fallbackBase)
					return false;

				g_luaModuleBase = fallbackBase;
				g_luaModuleEnd = fallbackBase + fallbackSize;
				return true;
			}

			bool ReadLuaModule(std::vector<uint8_t>& image)
			{
				if (!FindLuaModule())
					return false;

				const auto dos = ReadRemote<IMAGE_DOS_HEADER>(g_luaModuleBase);
				if (dos.e_magic != IMAGE_DOS_SIGNATURE)
					return false;
				const auto nt = ReadRemote<IMAGE_NT_HEADERS64>(g_luaModuleBase + dos.e_lfanew);
				if (nt.Signature != IMAGE_NT_SIGNATURE)
					return false;

				const size_t moduleSize = nt.OptionalHeader.SizeOfImage;
				g_luaModuleEnd = g_luaModuleBase + moduleSize;
				image.resize(moduleSize);
				SIZE_T read = 0;
				if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
					reinterpret_cast<LPCVOID>(g_luaModuleBase), image.data(), moduleSize, &read) || read < 0x1000)
					return false;
				image.resize(read);
				return true;
			}

			bool ResolveLuaApi()
			{
				if (g_luaLoad && g_luaPcall && g_luaModuleBase)
					return true;

				std::vector<uint8_t> image;
				if (!ReadLuaModule(image))
				{
					SetError("citizen-scripting-lua.dll not loaded. Stay in-game until resources finish starting.");
					return false;
				}

				g_luaLoad = FindExport(g_luaModuleBase, image, "lua_load");
				g_luaPcall = FindExport(g_luaModuleBase, image, "lua_pcallk");
				if (!g_luaPcall)
					g_luaPcall = FindExport(g_luaModuleBase, image, "lua_pcall");

				if (!g_luaLoad)
				{
					g_luaLoad = FindXrefFunction(image, g_luaModuleBase, "attempt to load a %s chunk (mode is '%s')");
					if (!g_luaLoad)
						g_luaLoad = FindXrefFunction(image, g_luaModuleBase, "attempt to load a %s chunk");
				}

				if (!g_luaPcall)
				{
					static const char* pcallHosts[] = {
						"Error loading script %s in resource %s: %s",
						"Error running script %s in resource %s: %s",
						"Error running script %s in resource %s",
						"Error loading script %s in resource %s",
					};
					for (const char* host : pcallHosts)
					{
						const uintptr_t hostFn = FindXrefFunction(image, g_luaModuleBase, host);
						if (!hostFn)
							continue;
						g_luaPcall = FirstInModuleCall(image, g_luaModuleBase, hostFn);
						if (g_luaPcall && g_luaPcall != g_luaLoad)
							break;
						g_luaPcall = 0;
					}
				}

				if (!g_luaLoad || !g_luaPcall || g_luaLoad == g_luaPcall)
				{
					g_luaLoad = 0;
					g_luaPcall = 0;
					SetError("Could not resolve lua_load/lua_pcall in citizen-scripting-lua.dll.");
					return false;
				}

				return true;
			}

			uintptr_t LuaStateFromContext(const CONTEXT& ctx, const std::vector<Region>& regions)
			{
				const uintptr_t regs[] = {
					static_cast<uintptr_t>(ctx.Rcx),
					static_cast<uintptr_t>(ctx.Rdx),
					static_cast<uintptr_t>(ctx.R8),
					static_cast<uintptr_t>(ctx.R9),
					static_cast<uintptr_t>(ctx.Rbx),
					static_cast<uintptr_t>(ctx.Rsi),
					static_cast<uintptr_t>(ctx.Rdi),
					static_cast<uintptr_t>(ctx.R12),
					static_cast<uintptr_t>(ctx.R13),
					static_cast<uintptr_t>(ctx.R14),
					static_cast<uintptr_t>(ctx.R15),
					static_cast<uintptr_t>(ctx.Rax),
				};
				for (uintptr_t value : regs)
				{
					if (LooksLikeLuaState(value, regions))
						return value;
				}
				return 0;
			}

			uintptr_t LuaStateFromStack(uintptr_t stackPointer, const std::vector<Region>& regions)
			{
				if (!stackPointer)
					return 0;
				for (int i = 0; i < 192; ++i)
				{
					const uintptr_t value = ReadRemote<uintptr_t>(stackPointer + static_cast<uintptr_t>(i) * 8);
					if (LooksLikeLuaState(value, regions))
						return value;
				}
				return 0;
			}

			uintptr_t PickLuaState(const CONTEXT& ctx, const std::string& resourceName)
			{
				const auto regions = EnumerateHeapRegions();
				if (const uintptr_t fromRegs = LuaStateFromContext(ctx, regions))
					return fromRegs;
				if (const uintptr_t fromStack = LuaStateFromStack(static_cast<uintptr_t>(ctx.Rsp), regions))
					return fromStack;

				std::vector<uintptr_t> fromStack;
				if (ctx.Rsp)
				{
					for (int i = 0; i < 192; ++i)
					{
						const uintptr_t value = ReadRemote<uintptr_t>(static_cast<uintptr_t>(ctx.Rsp) + static_cast<uintptr_t>(i) * 8);
						if (LooksLikeLuaState(value, regions) &&
							std::find(fromStack.begin(), fromStack.end(), value) == fromStack.end())
							fromStack.push_back(value);
					}
				}

				if (!resourceName.empty() && _stricmp(resourceName.c_str(), "any") != 0)
				{
					for (const auto& region : regions)
					{
						const size_t size = static_cast<size_t>(region.end - region.start);
						if (size > 6ull * 1024ull * 1024ull)
							continue;
						std::vector<uint8_t> buffer(size);
						SIZE_T read = 0;
						if (!ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
							reinterpret_cast<LPCVOID>(region.start), buffer.data(), size, &read) || read < resourceName.size() + 8)
							continue;

						for (size_t i = 0; i + resourceName.size() <= read; ++i)
						{
							if (std::memcmp(buffer.data() + i, resourceName.c_str(), resourceName.size()) != 0)
								continue;
							const size_t windowStart = i > 0x200 ? i - 0x200 : 0;
							const size_t windowEnd = (std::min)(read, i + 0x200);
							for (size_t offset = windowStart & ~size_t(7); offset + 8 <= windowEnd; offset += 8)
							{
								const uintptr_t candidate = *reinterpret_cast<uintptr_t*>(buffer.data() + offset);
								if (!LooksLikeLuaState(candidate, regions))
									continue;
								if (!fromStack.empty() &&
									std::find(fromStack.begin(), fromStack.end(), candidate) == fromStack.end())
									continue;
								return candidate;
							}
						}
					}
				}

				if (!fromStack.empty())
					return fromStack.front();
				return 0;
			}

#pragma pack(push, 1)
			struct RemoteLuaParams
			{
				uint64_t luaState = 0;
				uint64_t luaLoad = 0;
				uint64_t luaPcall = 0;
				uint64_t reader = 0;
				uint64_t loadS = 0;
				uint64_t namePtr = 0;
				uint64_t origRip = 0;
				uint64_t origRax = 0;
				uint64_t origRcx = 0;
				uint64_t origRdx = 0;
				uint64_t origR8 = 0;
				uint64_t origR9 = 0;
				uint64_t origRbx = 0;
				volatile int32_t done = 0;
				volatile int32_t result = 1;
			};

			struct LoadS
			{
				uint64_t ptr = 0;
				uint64_t len = 0;
			};
#pragma pack(pop)

			const uint8_t kReader[] = {
				0x48, 0x8B, 0x42, 0x08,
				0x48, 0x85, 0xC0,
				0x74, 0x0C,
				0x49, 0x89, 0x00,
				0x48, 0x8B, 0x02,
				0x48, 0xC7, 0x42, 0x08, 0x00, 0x00, 0x00, 0x00,
				0xC3,
				0x31, 0xC0,
				0x49, 0xC7, 0x00, 0x00, 0x00, 0x00, 0x00,
				0xC3
			};

			const uint8_t kStub[] = {
				0x53,
				0x48, 0x83, 0xEC, 0x40,
				0x48, 0x89, 0xCB,
				0x48, 0x8B, 0x0B,
				0x48, 0x8B, 0x53, 0x18,
				0x4C, 0x8B, 0x43, 0x20,
				0x4C, 0x8B, 0x4B, 0x28,
				0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00,
				0xFF, 0x53, 0x08,
				0x85, 0xC0,
				0x75, 0x20,
				0x48, 0x8B, 0x0B,
				0x31, 0xD2,
				0x45, 0x31, 0xC0,
				0x45, 0x31, 0xC9,
				0x48, 0xC7, 0x44, 0x24, 0x20, 0x00, 0x00, 0x00, 0x00,
				0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,
				0xFF, 0x53, 0x10,
				0x89, 0x43, 0x6C,
				0xC7, 0x43, 0x68, 0x01, 0x00, 0x00, 0x00,
				0x48, 0x83, 0xC4, 0x40,
				0x48, 0x83, 0x7B, 0x30, 0x00,
				0x74, 0x23,
				0x4C, 0x8B, 0x5B, 0x30,
				0x48, 0x8B, 0x43, 0x38,
				0x48, 0x8B, 0x4B, 0x40,
				0x48, 0x8B, 0x53, 0x48,
				0x4C, 0x8B, 0x43, 0x50,
				0x4C, 0x8B, 0x4B, 0x58,
				0x48, 0x8B, 0x5B, 0x60,
				0x48, 0x83, 0xC4, 0x08,
				0x41, 0xFF, 0xE3,
				0x5B,
				0xC3
			};

			bool RipInLua(uintptr_t rip)
			{
				return g_luaModuleBase && InRange(rip, g_luaModuleBase, g_luaModuleEnd);
			}

			bool StackHasLuaReturn(uintptr_t rsp)
			{
				if (!rsp || !g_luaModuleBase)
					return false;
				for (int i = 0; i < 192; ++i)
				{
					const uintptr_t value = ReadRemote<uintptr_t>(rsp + static_cast<uintptr_t>(i) * 8);
					if (InRange(value, g_luaModuleBase, g_luaModuleEnd))
						return true;
				}
				return false;
			}

			int ScoreScriptThread(const CONTEXT& ctx, const std::vector<Region>& regions, uintptr_t& luaStateOut)
			{
				luaStateOut = LuaStateFromContext(ctx, regions);
				if (!luaStateOut)
					luaStateOut = LuaStateFromStack(static_cast<uintptr_t>(ctx.Rsp), regions);

				const bool ripInLua = RipInLua(static_cast<uintptr_t>(ctx.Rip));
				const bool luaOnStack = StackHasLuaReturn(static_cast<uintptr_t>(ctx.Rsp));
				if (!luaStateOut && !ripInLua && !luaOnStack)
					return 0;

				int score = 0;
				if (ripInLua && luaStateOut)
					score = 5;
				else if (luaStateOut)
					score = 4;
				else if (ripInLua)
					score = 3;
				else if (luaOnStack)
					score = 1;
				return score;
			}

			HANDLE OpenScriptThread(CONTEXT& ctxOut, uintptr_t& luaStateOut)
			{
				HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
				if (snapshot == INVALID_HANDLE_VALUE)
					return nullptr;

				const auto regions = EnumerateHeapRegions();
				THREADENTRY32 entry{};
				entry.dwSize = sizeof(entry);
				HANDLE best = nullptr;
				int bestScore = 0;
				luaStateOut = 0;

				if (Thread32First(snapshot, &entry))
				{
					do
					{
						if (entry.th32OwnerProcessID != FrameWork::Memory::AttachedProcessPid)
							continue;

						HANDLE thread = OpenThread(THREAD_ALL_ACCESS, FALSE, entry.th32ThreadID);
						if (!thread)
							continue;

						if (SuspendThread(thread) == static_cast<DWORD>(-1))
						{
							CloseHandle(thread);
							continue;
						}

						CONTEXT ctx{};
						ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
						uintptr_t state = 0;
						int score = 0;
						if (GetThreadContext(thread, &ctx))
							score = ScoreScriptThread(ctx, regions, state);

						if (score > bestScore)
						{
							if (best)
							{
								ResumeThread(best);
								CloseHandle(best);
							}
							best = thread;
							bestScore = score;
							ctxOut = ctx;
							luaStateOut = state;
							continue;
						}

						ResumeThread(thread);
						CloseHandle(thread);
					} while (Thread32Next(snapshot, &entry));
				}

				CloseHandle(snapshot);
				return best;
			}

			bool WaitUntilNotInLua(HANDLE thread, CONTEXT& ctx)
			{
				for (int attempt = 0; attempt < 40; ++attempt)
				{
					if (!RipInLua(static_cast<uintptr_t>(ctx.Rip)))
						return true;

					ResumeThread(thread);
					Sleep(25);
					if (SuspendThread(thread) == static_cast<DWORD>(-1))
						return false;
					ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
					if (!GetThreadContext(thread, &ctx))
						return false;
				}
				return !RipInLua(static_cast<uintptr_t>(ctx.Rip));
			}

			bool WaitForDone(uintptr_t remoteParams, DWORD timeoutMs)
			{
				const DWORD start = GetTickCount();
				while (GetTickCount() - start < timeoutMs)
				{
					RemoteLuaParams current{};
					ReadProcessMemory(FrameWork::Memory::AttachedProcessHandle,
						reinterpret_cast<LPCVOID>(remoteParams), &current, sizeof(current), nullptr);
					if (current.done == 1)
						return current.result == 0;
					Sleep(20);
				}
				return false;
			}

			bool QueueSpecialApc(HANDLE thread, uintptr_t stub, uintptr_t params)
			{
				using QueueUserAPC2Fn = BOOL(WINAPI*)(PAPCFUNC, HANDLE, ULONG_PTR, ULONG);
				const auto queueUserApc2 = reinterpret_cast<QueueUserAPC2Fn>(
					GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "QueueUserAPC2"));
				if (queueUserApc2 &&
					queueUserApc2(reinterpret_cast<PAPCFUNC>(stub), thread, params, QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC))
					return true;

				using NtQueueApcThreadEx2Fn = LONG(NTAPI*)(HANDLE, HANDLE, ULONG, PVOID, PVOID, PVOID, PVOID);
				const auto ntQueueApcThreadEx2 = reinterpret_cast<NtQueueApcThreadEx2Fn>(
					GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueueApcThreadEx2"));
				if (ntQueueApcThreadEx2 &&
					ntQueueApcThreadEx2(thread, nullptr, QUEUE_USER_APC_FLAGS_SPECIAL_USER_APC,
						reinterpret_cast<PVOID>(stub), reinterpret_cast<PVOID>(params), nullptr, nullptr) >= 0)
					return true;

				using NtQueueApcThreadFn = LONG(NTAPI*)(HANDLE, PVOID, PVOID, PVOID, PVOID);
				const auto ntQueueApcThread = reinterpret_cast<NtQueueApcThreadFn>(
					GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueueApcThread"));
				if (!ntQueueApcThread)
					return false;
				return ntQueueApcThread(thread, reinterpret_cast<PVOID>(stub), reinterpret_cast<PVOID>(params), nullptr, nullptr) >= 0;
			}
		}

		bool IsAvailable()
		{
			return FrameWork::Memory::AttachedProcessHandle != nullptr;
		}

		std::string LastError()
		{
			std::lock_guard<std::mutex> lock(g_mutex);
			return g_lastError;
		}

		bool Execute(const std::string& luaCode, const std::string& resourceLabel)
		{
			if (luaCode.empty() || luaCode.size() > 96 * 1024)
			{
				SetError("Script is empty or larger than 96 KB.");
				return false;
			}
			if (!FrameWork::Memory::AttachedProcessHandle || !FrameWork::Memory::AttachedProcessPid)
			{
				SetError("FiveM is not attached.");
				return false;
			}

			std::lock_guard<std::mutex> lock(g_mutex);
			g_lastError.clear();
			if (!ResolveLuaApi())
				return false;

			CONTEXT threadCtx{};
			uintptr_t luaState = 0;
			HANDLE scriptThread = OpenScriptThread(threadCtx, luaState);
			if (!scriptThread)
			{
				SetError("No FiveM script thread found. Stay in-game until resources finish loading.");
				return false;
			}

			if (!WaitUntilNotInLua(scriptThread, threadCtx))
			{
				ResumeThread(scriptThread);
				CloseHandle(scriptThread);
				SetError("Script thread stayed inside Lua. Retry in a second.");
				return false;
			}

			if (!luaState)
				luaState = PickLuaState(threadCtx, resourceLabel);
			if (!luaState)
			{
				ResumeThread(scriptThread);
				CloseHandle(scriptThread);
				SetError("Found a script thread but no lua_State. Stay in-game and retry.");
				return false;
			}

			const size_t allocSize = sizeof(RemoteLuaParams) + sizeof(LoadS) + luaCode.size() + 64 + sizeof(kReader) + sizeof(kStub);
			void* remote = VirtualAllocEx(FrameWork::Memory::AttachedProcessHandle, nullptr, allocSize,
				MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
			if (!remote)
			{
				ResumeThread(scriptThread);
				CloseHandle(scriptThread);
				SetError("VirtualAllocEx failed in GTA process.");
				return false;
			}

			const uintptr_t remoteParams = reinterpret_cast<uintptr_t>(remote);
			const uintptr_t remoteLoadS = remoteParams + sizeof(RemoteLuaParams);
			const uintptr_t remoteCode = remoteLoadS + sizeof(LoadS);
			const uintptr_t remoteName = remoteCode + luaCode.size() + 1;
			const uintptr_t remoteReader = (remoteName + 16 + 15) & ~static_cast<uintptr_t>(15);
			const uintptr_t remoteStub = remoteReader + ((sizeof(kReader) + 15) & ~size_t(15));

			RemoteLuaParams params{};
			params.luaState = luaState;
			params.luaLoad = g_luaLoad;
			params.luaPcall = g_luaPcall;
			params.reader = remoteReader;
			params.loadS = remoteLoadS;
			params.namePtr = remoteName;
			params.origRip = 0;

			LoadS loadS{};
			loadS.ptr = remoteCode;
			loadS.len = luaCode.size();
			const char chunkName[] = "@t";

			bool wrote = WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteParams), &params, sizeof(params), nullptr)
				&& WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteLoadS), &loadS, sizeof(loadS), nullptr)
				&& WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteCode), luaCode.c_str(), luaCode.size() + 1, nullptr)
				&& WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteName), chunkName, sizeof(chunkName), nullptr)
				&& WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteReader), kReader, sizeof(kReader), nullptr)
				&& WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteStub), kStub, sizeof(kStub), nullptr);

			if (!wrote)
			{
				VirtualFreeEx(FrameWork::Memory::AttachedProcessHandle, remote, 0, MEM_RELEASE);
				ResumeThread(scriptThread);
				CloseHandle(scriptThread);
				SetError("WriteProcessMemory failed.");
				return false;
			}

			bool ok = false;
			bool threadResumed = false;
			if (QueueSpecialApc(scriptThread, remoteStub, remoteParams))
			{
				ResumeThread(scriptThread);
				threadResumed = true;
				ok = WaitForDone(remoteParams, 3000);
			}

			if (!ok)
			{
				if (threadResumed)
					SuspendThread(scriptThread);

				CONTEXT hijack{};
				hijack.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
				if (GetThreadContext(scriptThread, &hijack) &&
					!RipInLua(static_cast<uintptr_t>(hijack.Rip)))
				{
					params.origRip = hijack.Rip;
					params.origRax = hijack.Rax;
					params.origRcx = hijack.Rcx;
					params.origRdx = hijack.Rdx;
					params.origR8 = hijack.R8;
					params.origR9 = hijack.R9;
					params.origRbx = hijack.Rbx;
					params.done = 0;
					params.result = 1;
					WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteParams), &params, sizeof(params), nullptr);
					WriteProcessMemory(FrameWork::Memory::AttachedProcessHandle, reinterpret_cast<void*>(remoteLoadS), &loadS, sizeof(loadS), nullptr);

					hijack.Rip = remoteStub;
					hijack.Rcx = remoteParams;
					if (SetThreadContext(scriptThread, &hijack))
					{
						ResumeThread(scriptThread);
						threadResumed = true;
						ok = WaitForDone(remoteParams, 2500);
					}
				}

				if (!threadResumed)
					ResumeThread(scriptThread);
			}

			CloseHandle(scriptThread);
			if (ok)
			{
				VirtualFreeEx(FrameWork::Memory::AttachedProcessHandle, remote, 0, MEM_RELEASE);
				g_lastError.clear();
			}
			else
			{
				SetError("Lua ran on the script thread but timed out or returned an error. Retry in-game.");
			}
			return ok;
		}
	}
}

#else

namespace Cheat
{
	namespace LuaProcessInject
	{
		bool IsAvailable() { return false; }
		bool Execute(const std::string&, const std::string&) { return false; }
		std::string LastError() { return "Dev executor is not in this build."; }
	}
}

#endif
