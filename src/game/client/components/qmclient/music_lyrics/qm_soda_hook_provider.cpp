#include "qm_soda_hook_provider.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	constexpr wchar_t SHARED_MAPPING_NAME[] = L"Local\\QmClient.SodaHook.v1";
	constexpr int STOP_MAPPING_WAIT_MS = 500;

	std::wstring Utf8ToWide(const char *pText)
	{
		if(pText == nullptr || pText[0] == '\0')
			return {};
		const int Length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pText, -1, nullptr, 0);
		if(Length <= 1)
			return {};
		std::wstring Result((size_t)Length, L'\0');
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pText, -1, Result.data(), Length);
		Result.resize((size_t)Length - 1);
		return Result;
	}

	std::wstring DefaultHelperPath()
	{
		wchar_t aPath[MAX_PATH];
		const DWORD Length = GetModuleFileNameW(nullptr, aPath, (DWORD)std::size(aPath));
		if(Length == 0 || Length >= std::size(aPath))
			return {};
		std::wstring Path(aPath, Length);
		const size_t Slash = Path.find_last_of(L"\\/");
		if(Slash == std::wstring::npos)
			return L"qm-soda-helper.exe";
		Path.resize(Slash + 1);
		Path.append(L"qm-soda-helper.exe");
		return Path;
	}

	HANDLE LaunchHelper(const char *pHelperPath)
	{
		std::wstring Path = Utf8ToWide(pHelperPath);
		if(Path.empty())
			Path = DefaultHelperPath();
		if(Path.empty())
			return nullptr;
		const std::wstring CommandLine = L"\"" + Path + L"\" --parent-pid " + std::to_wstring(GetCurrentProcessId());
		STARTUPINFOW Startup{};
		Startup.cb = sizeof(Startup);
		PROCESS_INFORMATION Process{};
		std::vector<wchar_t> Mutable(CommandLine.begin(), CommandLine.end());
		Mutable.push_back(L'\0');
		if(CreateProcessW(Path.c_str(), Mutable.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, nullptr, nullptr, &Startup, &Process))
		{
			CloseHandle(Process.hThread);
			return Process.hProcess;
		}
		return nullptr;
	}
}

struct CQmSodaHookProvider::SImpl
{
	HANDLE m_hMapping = nullptr;
	void *m_pView = nullptr;
	HANDLE m_hHelperProcess = nullptr;
	bool m_HelperStarted = false;
	uint64_t m_LastSequence = 0;
	uint32_t m_LastPid = 0;

	bool OpenMapping(DWORD Access)
	{
		if(m_pView != nullptr)
			return true;
		m_hMapping = OpenFileMappingW(Access, FALSE, SHARED_MAPPING_NAME);
		if(m_hMapping == nullptr)
			return false;
		m_pView = MapViewOfFile(m_hMapping, Access, 0, 0, sizeof(QmSodaHook::SSharedBlock));
		if(m_pView == nullptr)
		{
			CloseHandle(m_hMapping);
			m_hMapping = nullptr;
			return false;
		}
		return true;
	}

	void CloseMapping()
	{
		if(m_pView != nullptr)
		{
			UnmapViewOfFile(m_pView);
			m_pView = nullptr;
		}
		if(m_hMapping != nullptr)
		{
			CloseHandle(m_hMapping);
			m_hMapping = nullptr;
		}
		m_LastSequence = 0;
		m_LastPid = 0;
	}
};

CQmSodaHookProvider::CQmSodaHookProvider() :
	m_pImpl(std::make_unique<SImpl>()) {}

CQmSodaHookProvider::~CQmSodaHookProvider()
{
	Stop();
}

void CQmSodaHookProvider::Start(const char *pHelperPath)
{
	if(!m_pImpl)
		return;
	if(!m_pImpl->m_HelperStarted)
	{
		m_pImpl->m_hHelperProcess = LaunchHelper(pHelperPath);
		m_pImpl->m_HelperStarted = m_pImpl->m_hHelperProcess != nullptr;
	}
}

void CQmSodaHookProvider::Stop()
{
	if(!m_pImpl)
		return;
	if(m_pImpl->m_hHelperProcess != nullptr)
	{
		if(WaitForSingleObject(m_pImpl->m_hHelperProcess, 0) == WAIT_TIMEOUT)
		{
			// Helper 是本客户端创建的子进程;关闭 Hook 数据源时结束它的监听循环。
			TerminateProcess(m_pImpl->m_hHelperProcess, 0);
			WaitForSingleObject(m_pImpl->m_hHelperProcess, 3000);
		}
		CloseHandle(m_pImpl->m_hHelperProcess);
		m_pImpl->m_hHelperProcess = nullptr;
	}
	m_pImpl->m_HelperStarted = false;
	m_pImpl->CloseMapping();
}

bool CQmSodaHookProvider::Read(QmSodaHook::SSnapshot *pSnapshot, int TimeoutMs)
{
	if(pSnapshot == nullptr || !m_pImpl)
		return false;
	if(m_pImpl->m_pView == nullptr && !m_pImpl->OpenMapping(FILE_MAP_READ))
		return false;
	QmSodaHook::SSnapshot Candidate{};
	bool Stable = false;
	for(int Attempt = 0; Attempt < 3; ++Attempt)
	{
		const auto *pShared = static_cast<const volatile QmSodaHook::SSharedBlock *>(m_pImpl->m_pView);
		const uint64_t Begin = *reinterpret_cast<const volatile uint64_t *>(&pShared->m_Sequence);
		MemoryBarrier();
		if(Begin == 0 || (Begin & 1) != 0)
			continue;
		std::memcpy(&Candidate, (const void *)&pShared->m_Snapshot, sizeof(Candidate));
		MemoryBarrier();
		const uint64_t End = *reinterpret_cast<const volatile uint64_t *>(&pShared->m_Sequence);
		if(!QmSodaHook::IsStableSequence(Begin, End) || Candidate.m_Sequence != End)
			continue;
		if(!QmSodaHook::ValidateSnapshot(Candidate))
			continue;
		const uint64_t Now = GetTickCount64();
		if(QmSodaHook::IsStale(Candidate, Now, (uint64_t)std::max(1, TimeoutMs)))
			return false;
		if(m_pImpl->m_LastPid != 0 && Candidate.m_SodaMusicPid != m_pImpl->m_LastPid)
			m_pImpl->m_LastSequence = 0;
		if(m_pImpl->m_LastSequence != 0 && Candidate.m_Sequence < m_pImpl->m_LastSequence)
			m_pImpl->m_LastSequence = 0;
		m_pImpl->m_LastPid = Candidate.m_SodaMusicPid;
		m_pImpl->m_LastSequence = Candidate.m_Sequence;
		*pSnapshot = Candidate;
		Stable = true;
		break;
	}
	return Stable;
}

bool CQmSodaHookProvider::IsRunning() const
{
	return m_pImpl != nullptr && m_pImpl->m_pView != nullptr;
}

#else

struct CQmSodaHookProvider::SImpl
{
};

CQmSodaHookProvider::CQmSodaHookProvider() :
	m_pImpl(std::make_unique<SImpl>()) {}
CQmSodaHookProvider::~CQmSodaHookProvider() = default;
void CQmSodaHookProvider::Start(const char *) {}
void CQmSodaHookProvider::Stop() {}
bool CQmSodaHookProvider::Read(QmSodaHook::SSnapshot *, int) { return false; }
bool CQmSodaHookProvider::IsRunning() const { return false; }

#endif
