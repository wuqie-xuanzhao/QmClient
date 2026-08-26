// 网易云音乐 Hook Helper：只发现并注入已经运行的主进程。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <bcrypt.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
	constexpr wchar_t TARGET_EXE[] = L"cloudmusic.exe";
	constexpr wchar_t TARGET_DLL[] = L"qm-nmt-hook64.dll";
	// 与 v4 共享映射配套，避免旧 Helper 抑制新协议的注入循环。
	constexpr wchar_t HELPER_MUTEX_NAME[] = L"Local\\QmClient.NeteaseHook.Helper.v4";
	constexpr char EXPECTED_EXE_SHA256[] = "1B86292DA1056A729226DF9205EB9F69C1E5558BC3EDDBE0639C06A2443BF2D3";
	constexpr char EXPECTED_CLOUDMUSIC_SHA256[] = "98874023CAB8D13F4E7E3FBC74FBC30186529039593E1359A189878ECE35029F";

	std::wstring Lower(std::wstring Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(), [](wchar_t C) { return (wchar_t)towlower(C); });
		return Value;
	}

	bool IsTargetArchitecture(HANDLE hProcess)
	{
		USHORT ProcessMachine = 0;
		USHORT NativeMachine = 0;
		using TIsWow64Process2 = BOOL(WINAPI *)(HANDLE, USHORT *, USHORT *);
		const auto pfn = reinterpret_cast<TIsWow64Process2>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));
		if(pfn == nullptr || !pfn(hProcess, &ProcessMachine, &NativeMachine))
			return false;
		return ProcessMachine == IMAGE_FILE_MACHINE_UNKNOWN && NativeMachine == IMAGE_FILE_MACHINE_AMD64;
	}

	std::string Sha256File(const std::wstring &Path)
	{
		HANDLE hFile = CreateFileW(Path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		if(hFile == INVALID_HANDLE_VALUE)
			return {};
		BCRYPT_ALG_HANDLE hAlgorithm = nullptr;
		BCRYPT_HASH_HANDLE hHash = nullptr;
		std::array<uint8_t, 32> Digest{};
		DWORD DigestLength = 0;
		DWORD ResultLength = 0;
		std::string Result;
		if(BCryptOpenAlgorithmProvider(&hAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
			BCryptGetProperty(hAlgorithm, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&DigestLength), sizeof(DigestLength), &ResultLength, 0) == 0 &&
			DigestLength == Digest.size() && BCryptCreateHash(hAlgorithm, &hHash, nullptr, 0, nullptr, 0, 0) == 0)
		{
			std::array<uint8_t, 1024 * 64> Buffer{};
			DWORD Read = 0;
			bool Good = true;
			while(ReadFile(hFile, Buffer.data(), (DWORD)Buffer.size(), &Read, nullptr) && Read > 0)
			{
				if(BCryptHashData(hHash, Buffer.data(), Read, 0) != 0)
				{
					Good = false;
					break;
				}
			}
			if(Good && BCryptFinishHash(hHash, Digest.data(), (ULONG)Digest.size(), 0) == 0)
			{
				static constexpr char Hex[] = "0123456789ABCDEF";
				for(const uint8_t Byte : Digest)
				{
					Result.push_back(Hex[Byte >> 4]);
					Result.push_back(Hex[Byte & 0x0F]);
				}
			}
		}
		if(hHash != nullptr)
			BCryptDestroyHash(hHash);
		if(hAlgorithm != nullptr)
			BCryptCloseAlgorithmProvider(hAlgorithm, 0);
		CloseHandle(hFile);
		return Result;
	}

	struct SCandidate
	{
		DWORD m_Pid = 0;
		DWORD m_ParentPid = 0;
		std::wstring m_Path;
		std::wstring m_CommandLine;
	};

	struct SUnicodeString
	{
		USHORT m_Length = 0;
		USHORT m_MaximumLength = 0;
		PWSTR m_Buffer = nullptr;
	};

	struct SProcessBasicInformation
	{
		PVOID m_Reserved1 = nullptr;
		PVOID m_PebBaseAddress = nullptr;
		PVOID m_Reserved2[2]{};
		ULONG_PTR m_UniqueProcessId = 0;
		PVOID m_Reserved3 = nullptr;
	};

	struct SPeb
	{
		BYTE m_Reserved1[2]{};
		BYTE m_BeingDebugged = 0;
		BYTE m_Reserved2[1]{};
		PVOID m_Reserved3[2]{};
		PVOID m_Ldr = nullptr;
		PVOID m_ProcessParameters = nullptr;
	};

	struct SProcessParametersPrefix
	{
		BYTE m_Reserved1[16]{};
		PVOID m_Reserved2[10]{};
		SUnicodeString m_ImagePathName;
		SUnicodeString m_CommandLine;
	};

	std::wstring QueryCommandLine(HANDLE hProcess)
	{
		using TNtQueryInformationProcess = LONG(NTAPI *)(HANDLE, ULONG, PVOID, ULONG, PULONG);
		const auto pfn = reinterpret_cast<TNtQueryInformationProcess>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess"));
		if(pfn == nullptr)
			return {};
		SProcessBasicInformation BasicInfo;
		ULONG ReturnLength = 0;
		if(pfn(hProcess, 0, &BasicInfo, sizeof(BasicInfo), &ReturnLength) != 0 || BasicInfo.m_PebBaseAddress == nullptr)
			return {};
		SPeb Peb;
		SIZE_T Read = 0;
		if(!ReadProcessMemory(hProcess, BasicInfo.m_PebBaseAddress, &Peb, sizeof(Peb), &Read) || Read != sizeof(Peb) || Peb.m_ProcessParameters == nullptr)
			return {};
		SProcessParametersPrefix Parameters;
		if(!ReadProcessMemory(hProcess, Peb.m_ProcessParameters, &Parameters, sizeof(Parameters), &Read) || Read != sizeof(Parameters) ||
			Parameters.m_CommandLine.m_Buffer == nullptr || Parameters.m_CommandLine.m_Length == 0)
			return {};
		const size_t CharCount = Parameters.m_CommandLine.m_Length / sizeof(wchar_t);
		std::wstring Result(CharCount, L'\0');
		if(!ReadProcessMemory(hProcess, Parameters.m_CommandLine.m_Buffer, Result.data(), Parameters.m_CommandLine.m_Length, &Read) ||
			Read != Parameters.m_CommandLine.m_Length)
			return {};
		return Result;
	}

	bool IsRendererCommandLine(const std::wstring &CommandLine)
	{
		const std::wstring LowerCommandLine = Lower(CommandLine);
		return LowerCommandLine.find(L"--type=") != std::wstring::npos || LowerCommandLine.find(L"--type ") != std::wstring::npos;
	}

	std::vector<SCandidate> FindCandidates()
	{
		std::vector<SCandidate> Result;
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if(hSnapshot == INVALID_HANDLE_VALUE)
			return Result;
		PROCESSENTRY32W Entry{};
		Entry.dwSize = sizeof(Entry);
		if(Process32FirstW(hSnapshot, &Entry))
		{
			do
			{
				if(Lower(Entry.szExeFile) != TARGET_EXE)
					continue;
				HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, Entry.th32ProcessID);
				if(hProcess == nullptr || !IsTargetArchitecture(hProcess))
				{
					if(hProcess != nullptr)
						CloseHandle(hProcess);
					continue;
				}
				std::wstring Path;
				for(DWORD Capacity = MAX_PATH; Capacity <= 32768; Capacity *= 2)
				{
					std::wstring Buffer(Capacity, L'\0');
					DWORD PathLength = Capacity;
					if(QueryFullProcessImageNameW(hProcess, 0, Buffer.data(), &PathLength))
					{
						Buffer.resize(PathLength);
						Path = std::move(Buffer);
						break;
					}
					if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
						break;
				}
				if(!Path.empty())
					Result.push_back({Entry.th32ProcessID, Entry.th32ParentProcessID, std::move(Path), QueryCommandLine(hProcess)});
				CloseHandle(hProcess);
			} while(Process32NextW(hSnapshot, &Entry));
		}
		CloseHandle(hSnapshot);
		return Result;
	}

	bool MatchesTargetBuild(const SCandidate &Candidate);

	std::optional<SCandidate> FindMainProcess()
	{
		const std::vector<SCandidate> Candidates = FindCandidates();
		for(const SCandidate &Candidate : Candidates)
		{
			if(IsRendererCommandLine(Candidate.m_CommandLine))
				continue;
			const bool HasCloudMusicParent = std::any_of(Candidates.begin(), Candidates.end(), [&Candidate](const SCandidate &Other) {
				return Candidate.m_ParentPid == Other.m_Pid && Other.m_Pid != Candidate.m_Pid;
			});
			if(!HasCloudMusicParent && MatchesTargetBuild(Candidate))
				return Candidate;
		}
		return std::nullopt;
	}

	std::wstring ModuleDirectory()
	{
		wchar_t aPath[MAX_PATH];
		const DWORD Length = GetModuleFileNameW(nullptr, aPath, (DWORD)std::size(aPath));
		if(Length == 0 || Length >= std::size(aPath))
			return {};
		std::wstring Path(aPath, Length);
		const size_t Slash = Path.find_last_of(L"\\/");
		return Slash == std::wstring::npos ? L"." : Path.substr(0, Slash);
	}

	bool MatchesTargetBuild(const SCandidate &Candidate)
	{
		const std::filesystem::path ExePath(Candidate.m_Path);
		const std::filesystem::path Directory = ExePath.parent_path();
		return Sha256File(ExePath.wstring()) == EXPECTED_EXE_SHA256 &&
		       Sha256File((Directory / L"cloudmusic.dll").wstring()) == EXPECTED_CLOUDMUSIC_SHA256;
	}

	bool IsAlreadyInjected(DWORD Pid, const std::wstring &DllPath)
	{
		const std::wstring FileName = Lower(std::filesystem::path(DllPath).filename().wstring());
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, Pid);
		if(hSnapshot == INVALID_HANDLE_VALUE)
			return false;
		MODULEENTRY32W Entry{};
		Entry.dwSize = sizeof(Entry);
		bool Found = false;
		if(Module32FirstW(hSnapshot, &Entry))
		{
			do
			{
				if(Lower(Entry.szModule) == FileName)
				{
					Found = true;
					break;
				}
			} while(Module32NextW(hSnapshot, &Entry));
		}
		CloseHandle(hSnapshot);
		return Found;
	}

	bool Inject(DWORD Pid, const std::wstring &DllPath)
	{
		HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, Pid);
		if(hProcess == nullptr || !IsTargetArchitecture(hProcess))
		{
			if(hProcess != nullptr)
				CloseHandle(hProcess);
			return false;
		}
		const size_t Bytes = (DllPath.size() + 1) * sizeof(wchar_t);
		void *pRemotePath = VirtualAllocEx(hProcess, nullptr, Bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if(pRemotePath == nullptr)
		{
			CloseHandle(hProcess);
			return false;
		}
		const bool Written = WriteProcessMemory(hProcess, pRemotePath, DllPath.c_str(), Bytes, nullptr) != FALSE;
		HANDLE hThread = Written ? CreateRemoteThread(hProcess, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")), pRemotePath, 0, nullptr) : nullptr;
		bool Success = false;
		if(hThread != nullptr)
		{
			if(WaitForSingleObject(hThread, 10000) == WAIT_OBJECT_0)
			{
				DWORD ExitCode = 0;
				Success = GetExitCodeThread(hThread, &ExitCode) && ExitCode != 0;
			}
			CloseHandle(hThread);
		}
		VirtualFreeEx(hProcess, pRemotePath, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return Success && IsAlreadyInjected(Pid, DllPath);
	}

	int Watch(const std::wstring &HookOverride, DWORD ParentPid)
	{
		const std::wstring Dir = ModuleDirectory();
		const std::wstring HookPath = HookOverride.empty() ? Dir + L"\\" + TARGET_DLL : HookOverride;
		if(GetFileAttributesW(HookPath.c_str()) == INVALID_FILE_ATTRIBUTES)
			return 3;
		HANDLE hParent = nullptr;
		if(ParentPid != 0)
		{
			hParent = OpenProcess(SYNCHRONIZE, FALSE, ParentPid);
			if(hParent == nullptr)
				return 4;
		}
		int Result = 0;
		for(;;)
		{
			if(hParent != nullptr && WaitForSingleObject(hParent, 0) == WAIT_OBJECT_0)
				break;
			const std::optional<SCandidate> Candidate = FindMainProcess();
			if(Candidate.has_value())
			{
				if(!IsAlreadyInjected(Candidate->m_Pid, HookPath))
					Inject(Candidate->m_Pid, HookPath);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		if(hParent != nullptr)
			CloseHandle(hParent);
		return Result;
	}
}

int wmain(int argc, wchar_t **argv)
{
	if(argc < 2 || std::wstring(argv[1]) != L"--watch")
		return 1;
	HANDLE hMutex = CreateMutexW(nullptr, TRUE, HELPER_MUTEX_NAME);
	if(hMutex == nullptr)
		return 2;
	if(GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		return 0;
	}
	std::wstring HookOverride;
	DWORD ParentPid = 0;
	for(int i = 2; i + 1 < argc; ++i)
	{
		if(std::wstring(argv[i]) == L"--hook")
			HookOverride = argv[++i];
		else if(std::wstring(argv[i]) == L"--parent-pid")
			ParentPid = (DWORD)_wtoi(argv[++i]);
	}
	const int Result = Watch(HookOverride, ParentPid);
	CloseHandle(hMutex);
	return Result;
}
