#include "Memory.hpp"
#include "../FiveM-External.hpp"

#include <winternl.h>
#include <TlHelp32.h>

struct TGetWindowHandleData
{
	DWORD Pid;
	std::wstring WindowName;
	HWND hWnd;
	LONG BestArea = 0;
};

static bool IsGameWindowClass(const char* className)
{
	return _stricmp(className, "grcWindow") == 0
		|| _stricmp(className, "grcWindowEx") == 0;
}

static bool IsIgnoredOverlayWindow(HWND Handle)
{
	if (!Handle || !IsWindow(Handle))
		return true;

	char ClassName[256]{};
	if (!GetClassNameA(Handle, ClassName, sizeof(ClassName)))
		return true;

	return _stricmp(ClassName, "ConsoleWindowClass") == 0
		|| _stricmp(ClassName, "CASCADIA_HOSTING_WINDOW_CLASS") == 0;
}

static bool IsFiveMGTAProcess(DWORD Pid)
{
	if (!Pid)
		return false;

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return false;

	bool match = false;
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	if (Process32FirstW(hSnapshot, &entry))
	{
		do
		{
			if (entry.th32ProcessID == Pid && wcsstr(entry.szExeFile, L"GTAProcess.exe"))
			{
				match = true;
				break;
			}
		} while (Process32NextW(hSnapshot, &entry));
	}

	CloseHandle(hSnapshot);
	return match;
}

static bool IsValidFiveMGameWindow(HWND Handle, DWORD requiredPid = 0)
{
	if (IsIgnoredOverlayWindow(Handle))
		return false;

	char ClassName[256]{};
	if (!GetClassNameA(Handle, ClassName, sizeof(ClassName)) || !IsGameWindowClass(ClassName))
		return false;

	DWORD windowPid = 0;
	GetWindowThreadProcessId(Handle, &windowPid);
	if (!windowPid || windowPid == GetCurrentProcessId())
		return false;

	(void)requiredPid;

	RECT Rect{};
	if (!GetWindowRect(Handle, &Rect))
		return false;

	return (Rect.right - Rect.left) >= 320 && (Rect.bottom - Rect.top) >= 240;
}

BOOL CALLBACK EnumWindowsCallback(HWND Handle, LPARAM lParam)
{
	TGetWindowHandleData& Data = *(TGetWindowHandleData*)lParam;

	if (Data.Pid == 0)
	{
		int Length = SafeCall(GetWindowTextLength)(Handle);
		if (Length == 0)
			return true;

		std::wstring Buffer(Length + 1, L'\0');

		SafeCall(GetWindowText)(Handle, &Buffer[0], Length + 1);

		if (Data.WindowName != Buffer)
			return true;

		Data.hWnd = Handle;
		return false;
	}

	DWORD Pid;
	SafeCall(GetWindowThreadProcessId)(Handle, &Pid);

	if (Data.Pid != Pid)
		return true;

	if (IsIgnoredOverlayWindow(Handle))
		return true;

	char ClassName[256]{};
	GetClassNameA(Handle, ClassName, sizeof(ClassName));
	const bool isGameClass = IsGameWindowClass(ClassName);

	RECT Rect{};
	if (!GetWindowRect(Handle, &Rect))
		return true;

	const LONG Area = (Rect.right - Rect.left) * (Rect.bottom - Rect.top);
	if (Area <= 0)
		return true;

	if (Data.hWnd)
	{
		char CurrentClass[256]{};
		GetClassNameA(Data.hWnd, CurrentClass, sizeof(CurrentClass));
		const bool currentIsGameClass = IsGameWindowClass(CurrentClass);
		if (currentIsGameClass && !isGameClass)
			return true;
		if (isGameClass == currentIsGameClass && Area <= Data.BestArea)
			return true;
	}

	Data.hWnd = Handle;
	Data.BestArea = Area;
	return true;
}

struct FindGameWindowData
{
	DWORD GtaPid = 0;
	HWND Result = nullptr;
};

BOOL CALLBACK FindGameWindowCallback(HWND Handle, LPARAM lParam)
{
	auto* Data = reinterpret_cast<FindGameWindowData*>(lParam);
	if (IsValidFiveMGameWindow(Handle, Data->GtaPid))
	{
		Data->Result = Handle;
		return FALSE;
	}

	return TRUE;
}

namespace FrameWork
{
	namespace Memory
	{
		HWND Memory::GetWindowHandleByPID(DWORD Pid)
		{
			TGetWindowHandleData HandleData;
			HandleData.Pid = Pid;
			HandleData.WindowName = XorStr(L"WindowName001");
			HandleData.hWnd = NULL;
			HandleData.BestArea = 0;

			SafeCall(EnumWindows)(EnumWindowsCallback, (LPARAM)&HandleData);
			return HandleData.hWnd;
		}

		DWORD Memory::GetFiveMGTAProcessPid()
		{
			HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
			if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE)
				return 0;

			DWORD gtaPid = 0;
			PROCESSENTRY32 ProcessEntry{};
			ProcessEntry.dwSize = sizeof(ProcessEntry);
			if (SafeCall(Process32First)(hSnapshot, &ProcessEntry))
			{
				do
				{
					if (wcsstr(ProcessEntry.szExeFile, L"GTAProcess.exe"))
					{
						gtaPid = ProcessEntry.th32ProcessID;
						break;
					}
				} while (SafeCall(Process32Next)(hSnapshot, &ProcessEntry));
			}

			SafeCall(CloseHandle)(hSnapshot);
			return gtaPid;
		}

		bool Memory::FindFiveMGameProcess(DWORD& outPid, std::wstring& outName)
		{
			outPid = 0;
			outName.clear();

			HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
			if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE)
				return false;

			DWORD gtaPid = 0;
			DWORD gamePid = 0;
			std::wstring gtaName;
			std::wstring gameName;

			PROCESSENTRY32W entry{};
			entry.dwSize = sizeof(entry);
			if (SafeCall(Process32FirstW)(hSnapshot, &entry))
			{
				do
				{
					if (wcsstr(entry.szExeFile, L"GTAProcess.exe"))
					{
						gtaPid = entry.th32ProcessID;
						gtaName = entry.szExeFile;
						break;
					}
					if (!gamePid && wcsstr(entry.szExeFile, L"GameProcess.exe"))
					{
						gamePid = entry.th32ProcessID;
						gameName = entry.szExeFile;
					}
				} while (SafeCall(Process32NextW)(hSnapshot, &entry));
			}

			SafeCall(CloseHandle)(hSnapshot);

			if (gtaPid)
			{
				outPid = gtaPid;
				outName = gtaName;
				return true;
			}
			if (gamePid)
			{
				outPid = gamePid;
				outName = gameName;
				return true;
			}
			return false;
		}

		DWORD Memory::GetFiveMGameProcessPid()
		{
			HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
			if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE)
				return 0;

			DWORD gamePid = 0;
			PROCESSENTRY32 ProcessEntry{};
			ProcessEntry.dwSize = sizeof(ProcessEntry);
			if (SafeCall(Process32First)(hSnapshot, &ProcessEntry))
			{
				do
				{
					if (wcsstr(ProcessEntry.szExeFile, L"GameProcess.exe"))
					{
						gamePid = ProcessEntry.th32ProcessID;
						break;
					}
				} while (SafeCall(Process32Next)(hSnapshot, &ProcessEntry));
			}

			SafeCall(CloseHandle)(hSnapshot);
			return gamePid;
		}

		bool Memory::IsAnyFiveMRunning()
		{
			return GetFiveMGTAProcessPid() != 0 || GetFiveMGameProcessPid() != 0;
		}

		bool Memory::IsGameSessionActive()
		{
			if (GetFiveMGTAProcessPid())
				return true;

			const char* windowClasses[] = { "grcWindow", "grcWindowEx", nullptr };
			for (int i = 0; windowClasses[i]; ++i)
			{
				HWND handle = FindWindowA(windowClasses[i], nullptr);
				if (handle && IsWindow(handle))
					return true;
			}

			return false;
		}

		bool Memory::IsOverlayTargetReady()
		{
			HWND handle = FindGameWindow();
			if (!handle || !IsWindow(handle) || !IsWindowVisible(handle) || IsIconic(handle))
				return false;

			RECT client{};
			if (!GetClientRect(handle, &client))
				return false;

			return (client.right - client.left) >= 640 && (client.bottom - client.top) >= 480;
		}

		HWND Memory::FindGameWindow(DWORD PreferredPid)
		{
			(void)PreferredPid;

			const DWORD gtaPid = GetFiveMGTAProcessPid();
			const char* windowClasses[] = { "grcWindow", "grcWindowEx", nullptr };
			for (int i = 0; windowClasses[i]; ++i)
			{
				HWND handle = FindWindowA(windowClasses[i], nullptr);
				if (IsValidFiveMGameWindow(handle, gtaPid))
					return handle;
			}

			if (gtaPid)
			{
				HWND handle = GetWindowHandleByPID(gtaPid);
				if (IsValidFiveMGameWindow(handle, gtaPid))
					return handle;
			}

			FindGameWindowData data{};
			data.GtaPid = gtaPid;
			SafeCall(EnumWindows)(FindGameWindowCallback, (LPARAM)&data);
			if (data.Result)
				return data.Result;

			HWND fallback = FindWindowA("grcWindow", nullptr);
			if (fallback && IsWindow(fallback))
				return fallback;
			fallback = FindWindowA("grcWindowEx", nullptr);
			return (fallback && IsWindow(fallback)) ? fallback : nullptr;
		}

		bool Memory::IsProcessRunning(DWORD pid)
		{
			if (!pid)
				return false;
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
			if (!hProcess)
				return false;

			DWORD exitCode = 0;
			bool running = GetExitCodeProcess(hProcess, &exitCode) && exitCode == STILL_ACTIVE;
			CloseHandle(hProcess);
			return running;
		}

		DWORD Memory::GetProcessPidByName(std::wstring ProcessName)
		{
			HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
			if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE || hSnapshot == ((HANDLE)(LONG_PTR)ERROR_BAD_LENGTH))
			{
				return 0;
			}

			DWORD Pid;
			PROCESSENTRY32 ProcessEntry;
			ProcessEntry.dwSize = sizeof(ProcessEntry);
			if (SafeCall(Process32First)(hSnapshot, &ProcessEntry))
			{
				while (_wcsicmp(ProcessEntry.szExeFile, ProcessName.c_str()))
				{
					if (!SafeCall(Process32Next)(hSnapshot, &ProcessEntry))
					{
						SafeCall(CloseHandle)(hSnapshot);
						return 0;
					}
				}

				Pid = ProcessEntry.th32ProcessID;
			}
			else
			{
				SafeCall(CloseHandle)(hSnapshot);
				return 0;
			}

			SafeCall(CloseHandle)(hSnapshot);
			return Pid;
		}

		bool Memory::GetModuleInfoByName(DWORD Pid, std::wstring ModuleName, uint64_t& ModuleBase, uint32_t& ModuleSize)
		{
			ModuleBase = 0;
			ModuleSize = 0;

			for (int attempt = 0; attempt < 8; ++attempt)
			{
				HANDLE hSnapshot = SafeCall(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, Pid);
				if (!hSnapshot || hSnapshot == INVALID_HANDLE_VALUE)
				{
					Sleep(20);
					continue;
				}

				MODULEENTRY32W ModuleEntry{};
				ModuleEntry.dwSize = sizeof(ModuleEntry);
				if (SafeCall(Module32FirstW)(hSnapshot, &ModuleEntry))
				{
					do
					{
						if (_wcsicmp(ModuleEntry.szModule, ModuleName.c_str()) == 0)
						{
							ModuleBase = reinterpret_cast<uint64_t>(ModuleEntry.modBaseAddr);
							ModuleSize = ModuleEntry.modBaseSize;
							SafeCall(CloseHandle)(hSnapshot);
							return ModuleBase != 0;
						}
					} while (SafeCall(Module32NextW)(hSnapshot, &ModuleEntry));
				}

				SafeCall(CloseHandle)(hSnapshot);
				Sleep(20);
			}

			return false;
		}

		uint64_t Memory::GetModuleBaseByName(DWORD Pid, std::wstring ModuleName)
		{
			uint64_t base = 0;
			uint32_t size = 0;
			GetModuleInfoByName(Pid, std::move(ModuleName), base, size);
			return base;
		}

		uint64_t Memory::GetModuleSizeByName(DWORD Pid, std::wstring ModuleName)
		{
			uint64_t base = 0;
			uint32_t size = 0;
			GetModuleInfoByName(Pid, std::move(ModuleName), base, size);
			return size;
		}

		void Memory::AttachProces(DWORD Pid)
		{
			AttachedProcessHandle = SafeCall(OpenProcess)(PROCESS_ALL_ACCESS, false, Pid);
			AttachedProcessPid = Pid;
			ProcHandle = AttachedProcessHandle; // alias used by FindSignature
		}

		void Memory::SetModuleInfo(uintptr_t Base, uintptr_t BaseSize)
		{
			ModBase     = Base;
			ModBaseSize = BaseSize;
		}

		void Memory::ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size)
		{
			if (AttachedProcessHandle && AttachedProcessPid)
			{
				static pNtReadVirtualMemory NtReadVirtualMemory = (pNtReadVirtualMemory)SafeCall(GetProcAddress)(SafeCall(GetModuleHandleA)(XorStr("ntdll.dll")), XorStr("NtReadVirtualMemory"));

				NTSTATUS Status = NtReadVirtualMemory(AttachedProcessHandle, (PVOID)ReadAddress, Read, Size, nullptr);

				if (Status == 0)
					return;
			}
			return;
		}

		std::string Memory::ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize)
		{
			const int BufferSize = 256;

			char Buffer[BufferSize];

			int BytesRead = 0;

			while (BytesRead < BufferSize && BytesRead < StringSize)
			{
				char Character;
				ReadProcessMemoryImpl((uint64_t)ReadAddress + BytesRead, &Character, sizeof(char));
				Buffer[BytesRead] = Character;

				if (Character == '\0')
					break;

				BytesRead++;
			}

			if (BytesRead < BufferSize)
				Buffer[BytesRead] = '\0';
			else
				Buffer[BufferSize - 1] = '\0';

			return std::string(Buffer);
		}

		bool Memory::WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Value, SIZE_T Size)
		{
			if (!AttachedProcessHandle || !AttachedProcessPid || !WriteAddress || !Value || !Size)
				return false;

			DWORD oldProtect = 0;
			VirtualProtectEx(AttachedProcessHandle, (LPVOID)WriteAddress, Size, PAGE_EXECUTE_READWRITE, &oldProtect);

			static pNtWriteVirtualMemory NtWriteVirtualMemory = (pNtWriteVirtualMemory)SafeCall(GetProcAddress)(SafeCall(GetModuleHandleA)(XorStr("ntdll.dll")), XorStr("NtWriteVirtualMemory"));

			NTSTATUS Status = NtWriteVirtualMemory(AttachedProcessHandle, (PVOID)WriteAddress, Value, Size, nullptr);

			DWORD ignored = 0;
			if (oldProtect)
				VirtualProtectEx(AttachedProcessHandle, (LPVOID)WriteAddress, Size, oldProtect, &ignored);

			return Status == 0;
		}

		bool Memory::HookJump(uintptr_t HookAddress, uintptr_t JmpToAddress)
		{
			BYTE JumpPatch[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };

			for (int x = 0; x < sizeof(JumpPatch); x++)
			{
				WriteMemory<BYTE>(HookAddress + x, JumpPatch[x]);
			}

			WriteMemory<uintptr_t>(HookAddress + 6, JmpToAddress);

			return true;
		}

		std::vector<uint8_t> Memory::ReadBytes(uintptr_t Addr, size_t Size)
		{
			std::vector<uint8_t> Bytes(Size);
			size_t BytesRead = 0;
			ReadProcessMemory(AttachedProcessHandle, (LPCVOID)Addr, Bytes.data(), Size, &BytesRead);
			Bytes.resize(BytesRead);
			return Bytes;
		}

		bool Memory::WriteBytes(uintptr_t Addr, std::vector<uint8_t> Bytes)
		{
			if (!AttachedProcessHandle || Bytes.empty())
				return false;

			DWORD oldProtect = 0;
			VirtualProtectEx(AttachedProcessHandle, (LPVOID)Addr, Bytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect);

			NTSTATUS Status = WriteProcessMemory(AttachedProcessHandle, (LPVOID)Addr, Bytes.data(), Bytes.size(), NULL);
			const bool success = NT_SUCCESS(Status);

			DWORD ignored = 0;
			if (oldProtect)
				VirtualProtectEx(AttachedProcessHandle, (LPVOID)Addr, Bytes.size(), oldProtect, &ignored);

			return success;
		}

		bool Memory::PatchFunc(uintptr_t Addr, int NopCount)
		{
			if (!NopCount)
				return false;

			std::vector<uint8_t> PatchTable = {};
			PatchTable.reserve(NopCount);

			for (int i = 0; i < NopCount; i++)
			{
				PatchTable.push_back(0x90);
			}

			return WriteBytes(Addr, PatchTable);
		}
		/*
		 * Old FindSignature implementations removed.
		 */
		uint64_t Memory::FindSignature(std::vector<uint8_t> Signature, uint64_t ModuleBase, uint64_t ModuleBaseSize)
		{
			if (!AttachedProcessHandle || !AttachedProcessPid)
				return 0;

			static pNtReadVirtualMemory NtReadVirtualMemory = (pNtReadVirtualMemory)SafeCall(GetProcAddress)(SafeCall(GetModuleHandleA)(XorStr("ntdll.dll")), XorStr("NtReadVirtualMemory"));

			const size_t blockSize = 4096 * 4;
			std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(blockSize);
			size_t signatureSize = Signature.size();

			uint64_t Base = ModuleBase     ? ModuleBase     : ModBase;
			uint64_t Size = ModuleBaseSize ? ModuleBaseSize : ModBaseSize;

			if (!Base || !Size)
				return 0;

			for (uint64_t address = Base; address < Base + Size; )
			{
				SIZE_T bytesRead = 0;
				if (NtReadVirtualMemory(AttachedProcessHandle, (PVOID)address, data.get(), blockSize, &bytesRead) != 0 || bytesRead == 0)
				{
					address += blockSize;
					continue;
				}

				for (uint64_t i = 0; i < bytesRead; i++)
				{
					uint64_t j = 0;
					for (; j < signatureSize; j++)
					{
						if (Signature[j] == 0x00)
							continue;
						if (i + j >= bytesRead)
							break;
						if (data[i + j] != Signature[j])
							break;
					}
					if (j == signatureSize)
						return address + i;
				}

				const uint64_t overlap = signatureSize > 1 ? signatureSize - 1 : 0;
				address += (bytesRead > overlap) ? (bytesRead - overlap) : bytesRead;
			}

			return 0;
		}

		uint64_t Memory::FindSignatureFrom(std::vector<uint8_t> Signature, uint64_t StartAddress, uint64_t ModuleBase, uint64_t ModuleBaseSize)
		{
			if (!AttachedProcessHandle || !AttachedProcessPid || !StartAddress)
				return 0;

			static pNtReadVirtualMemory NtReadVirtualMemory = (pNtReadVirtualMemory)SafeCall(GetProcAddress)(SafeCall(GetModuleHandleA)(XorStr("ntdll.dll")), XorStr("NtReadVirtualMemory"));

			const size_t blockSize = 4096 * 4;
			std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(blockSize);
			size_t signatureSize = Signature.size();

			uint64_t Base = ModuleBase ? ModuleBase : ModBase;
			uint64_t Size = ModuleBaseSize ? ModuleBaseSize : ModBaseSize;

			if (!Base || !Size || StartAddress < Base)
				return 0;

			const uint64_t endAddress = Base + Size;
			for (uint64_t address = StartAddress; address < endAddress; )
			{
				SIZE_T bytesRead = 0;
				if (NtReadVirtualMemory(AttachedProcessHandle, (PVOID)address, data.get(), blockSize, &bytesRead) != 0 || bytesRead == 0)
				{
					address += blockSize;
					continue;
				}

				for (uint64_t i = 0; i < bytesRead; i++)
				{
					uint64_t j = 0;
					for (; j < signatureSize; j++)
					{
						if (Signature[j] == 0x00)
							continue;
						if (i + j >= bytesRead)
							break;
						if (data[i + j] != Signature[j])
							break;
					}
					if (j == signatureSize)
						return address + i;
				}

				const uint64_t overlap = signatureSize > 1 ? signatureSize - 1 : 0;
				address += (bytesRead > overlap) ? (bytesRead - overlap) : bytesRead;
			}

			return 0;
		}

		uint64_t Memory::FindSignatureStr(const char* Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize)
		{
			std::vector<std::pair<uint8_t, bool>> pattern;
			const char* p = Signature;
			while (*p) {
				while (*p == ' ') p++;
				if (!*p) break;
				if (p[0] == '?' ) {
					pattern.push_back({ 0, true });
					p++; if (*p == '?') p++;
				} else {
					uint8_t byte = (uint8_t)strtol(p, nullptr, 16);
					pattern.push_back({ byte, false });
					p += 2;
				}
			}

			const size_t blockSize = 0x1000 * 4;
			std::unique_ptr<uint8_t[]> data = std::make_unique<uint8_t[]>(blockSize);
			size_t sigSize = pattern.size();

			static pNtReadVirtualMemory NtReadVirtualMemory = (pNtReadVirtualMemory)SafeCall(GetProcAddress)(SafeCall(GetModuleHandleA)(XorStr("ntdll.dll")), XorStr("NtReadVirtualMemory"));
			if (!NtReadVirtualMemory) return 0;

			for (uintptr_t address = ModuleBase; address < ModuleBase + ModuleBaseSize; address += blockSize) {
				SIZE_T bytesRead = 0;
				if (NtReadVirtualMemory(AttachedProcessHandle, (PVOID)address, data.get(), blockSize, &bytesRead) < 0 || bytesRead == 0)
					continue;
				for (uintptr_t i = 0; i < bytesRead; i++) {
					uintptr_t j = 0;
					for (; j < sigSize; j++) {
						if (i + j >= bytesRead) break;
						if (!pattern[j].second && data[i + j] != pattern[j].first) break;
					}
					if (j == sigSize) return address + i;
				}
			}
			return 0;
		}

		uintptr_t Memory::ResolveRelativeAddress(uintptr_t Instruction, int OffsetOffset, int InstructionSize)
		{
			if (Instruction == 0)
				return 0;
			int32_t rip_offset = ReadMemory<int32_t>(Instruction + OffsetOffset);
			return Instruction + InstructionSize + rip_offset;
		}
		uint64_t Memory::CreateCodeCave(size_t Size)
		{
			return AllocateCave(Size);
		}

		uint64_t Memory::AllocateCave(size_t Size)
		{
			LPVOID CaveAddress = nullptr;
			if (ModBase && ModBaseSize)
				CaveAddress = VirtualAllocEx(AttachedProcessHandle, (LPVOID)(ModBase + ModBaseSize + 0x10000), Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

			if (!CaveAddress)
				CaveAddress = VirtualAllocEx(AttachedProcessHandle, NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);

			return (uint64_t)CaveAddress;
		}

		uint64_t Memory::AllocateCaveNear(uint64_t NearAddress, size_t Size)
		{
			if (!AttachedProcessHandle || !NearAddress || !Size)
				return 0;

			SYSTEM_INFO sysInfo{};
			GetSystemInfo(&sysInfo);
			const size_t granularity = sysInfo.dwAllocationGranularity ? sysInfo.dwAllocationGranularity : 0x1000;

			auto tryAlloc = [&](uint64_t address) -> uint64_t
			{
				LPVOID result = VirtualAllocEx(
					AttachedProcessHandle,
					(LPVOID)address,
					Size,
					MEM_COMMIT | MEM_RESERVE,
					PAGE_EXECUTE_READWRITE);
				return (uint64_t)result;
			};

			for (size_t offset = granularity; offset < 0x70000000ULL; offset += granularity)
			{
				for (int direction = -1; direction <= 1; direction += 2)
				{
					const int64_t candidate = static_cast<int64_t>(NearAddress) + static_cast<int64_t>(direction) * static_cast<int64_t>(offset);
					if (candidate <= 0)
						continue;

					if (const uint64_t allocated = tryAlloc(static_cast<uint64_t>(candidate)))
						return allocated;
				}
			}

			return AllocateCave(Size);
		}

		bool Memory::FreeCave(uintptr_t Addr)
		{
			return VirtualFreeEx(AttachedProcessHandle, (LPVOID)Addr, 0, MEM_RELEASE);
		}
	}
}