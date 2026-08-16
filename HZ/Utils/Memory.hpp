#pragma once

#include <Windows.h>
#include <winternl.h>
#include <vector>
#include <string>
#include <memory>

typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize, PSIZE_T NumberOfBytesWritten);
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T BufferSize, PSIZE_T NumberOfBytesRead);

namespace FrameWork
{
	namespace Memory
	{
		HWND GetWindowHandleByPID(DWORD Pid);
		HWND FindGameWindow(DWORD PreferredPid = 0);
		DWORD GetFiveMGTAProcessPid();
		DWORD GetFiveMGameProcessPid();
		bool FindFiveMGameProcess(DWORD& outPid, std::wstring& outName);
		bool IsAnyFiveMRunning();
		bool IsGameSessionActive();
		bool IsOverlayTargetReady();

		DWORD GetProcessPidByName(std::wstring ProcessName);
		bool IsProcessRunning(DWORD pid);

		uint64_t GetModuleBaseByName(DWORD Pid, std::wstring ModuleName);
		uint64_t GetModuleSizeByName(DWORD Pid, std::wstring ModuleName);
		bool GetModuleInfoByName(DWORD Pid, std::wstring ModuleName, uint64_t& ModuleBase, uint32_t& ModuleSize);

		uint64_t CreateCodeCave(size_t Size);
		uint64_t AllocateCave(size_t Size);
		uint64_t AllocateCaveNear(uint64_t NearAddress, size_t Size);
		bool FreeCave(uintptr_t Addr);

		std::vector<uint8_t> ReadBytes(uintptr_t Addr, size_t Size);
		bool WriteBytes(uintptr_t Addr, std::vector<uint8_t> Bytes);
		bool PatchFunc(uintptr_t Addr, int NopCount);

		bool HookJump(uintptr_t HookAddress, uintptr_t JmpToAddress);
		uint64_t FindSignature(std::vector<uint8_t> Signature, uint64_t ModuleBase = 0, uint64_t ModuleBaseSize = 0);
		uint64_t FindSignatureFrom(std::vector<uint8_t> Signature, uint64_t StartAddress, uint64_t ModuleBase = 0, uint64_t ModuleBaseSize = 0);
		uint64_t FindSignatureStr(const char* Signature, uintptr_t ModuleBase, uintptr_t ModuleBaseSize);
		uintptr_t ResolveRelativeAddress(uintptr_t Instruction, int OffsetOffset, int InstructionSize);

		void AttachProces(DWORD Pid);
		void SetModuleInfo(uintptr_t Base, uintptr_t BaseSize); // sets ModBase/ModBaseSize for FindSignature
		std::string ReadProcessMemoryString(uint64_t ReadAddress, SIZE_T StringSize = 256);
		void ReadProcessMemoryImpl(uint64_t ReadAddress, LPVOID Read, SIZE_T Size);
		bool WriteProcessMemoryImpl(uint64_t WriteAddress, LPVOID Write, SIZE_T Size);

		template <typename T, typename B>
		T ReadMemory(B ReadAddress)
		{
			T Read;
			ReadProcessMemoryImpl((uint64_t)ReadAddress, &Read, sizeof(T));
			return Read;
		}

		template <typename T, typename B>
		bool WriteMemory(B WriteAddress, T Value)
		{
			return WriteProcessMemoryImpl((uint64_t)WriteAddress, &Value, sizeof(T));
		}

		inline HANDLE AttachedProcessHandle;
		inline DWORD AttachedProcessPid;

		// Used by FindSignature when no explicit module base is provided
		inline uintptr_t ModBase     = 0;
		inline uintptr_t ModBaseSize = 0;
		inline HANDLE    ProcHandle  = nullptr;
	}
}