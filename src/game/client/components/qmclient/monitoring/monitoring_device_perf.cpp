// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "monitoring.h"

#include <base/system.h>

#include <chrono>
#include <cwchar>
#include <utility>
#include <vector>

#if defined(CONF_FAMILY_WINDOWS)
#include <windows.h>

#define IStorage IStorageCOM
#include <dxgi1_4.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>
#undef IStorage

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#endif

namespace
{

#if defined(CONF_FAMILY_WINDOWS)
	class CWindowsDevicePerfSource
	{
		bool m_GpuCountersAttemptedInit = false;
		bool m_GpuCountersAvailable = false;
		bool m_GpuCountersPrimed = false;
		PDH_HQUERY m_Query = nullptr;
		std::vector<PDH_HCOUNTER> m_vCounters;
		bool m_AdapterAttemptedInit = false;
		IDXGIAdapter3 *m_pAdapter = nullptr;
		uint64_t m_LastReadBytes = 0;
		uint64_t m_LastReadTickNs = 0;

		void EnsureGpuCountersInitialized()
		{
			if(m_GpuCountersAttemptedInit)
				return;
			m_GpuCountersAttemptedInit = true;
			if(PdhOpenQueryW(nullptr, 0, &m_Query) != ERROR_SUCCESS)
				return;

			DWORD PathSize = 0;
			if(PdhExpandWildCardPathW(nullptr, L"\\GPU Engine(*)\\Utilization Percentage", nullptr, &PathSize, 0) != PDH_MORE_DATA || PathSize == 0)
				return;

			std::vector<wchar_t> vPaths(PathSize);
			if(PdhExpandWildCardPathW(nullptr, L"\\GPU Engine(*)\\Utilization Percentage", vPaths.data(), &PathSize, 0) != ERROR_SUCCESS)
				return;

			for(const wchar_t *pPath = vPaths.data(); *pPath != L'\0'; pPath += std::wcslen(pPath) + 1)
			{
				PDH_HCOUNTER Counter = nullptr;
				if(PdhAddEnglishCounterW(m_Query, pPath, 0, &Counter) == ERROR_SUCCESS)
					m_vCounters.push_back(Counter);
			}
			m_GpuCountersAvailable = !m_vCounters.empty();
			if(m_GpuCountersAvailable)
			{
				PdhCollectQueryData(m_Query);
				m_GpuCountersPrimed = true;
			}
		}

		void EnsureAdapterInitialized()
		{
			if(m_AdapterAttemptedInit)
				return;
			m_AdapterAttemptedInit = true;
			IDXGIFactory1 *pFactory = nullptr;
			if(SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&pFactory))))
			{
				IDXGIAdapter1 *pAdapter1 = nullptr;
				if(SUCCEEDED(pFactory->EnumAdapters1(0, &pAdapter1)))
				{
					pAdapter1->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void **>(&m_pAdapter));
					pAdapter1->Release();
				}
				pFactory->Release();
			}
		}

	public:
		~CWindowsDevicePerfSource()
		{
			for(PDH_HCOUNTER Counter : m_vCounters)
			{
				if(Counter != nullptr)
					PdhRemoveCounter(Counter);
			}
			m_vCounters.clear();
			if(m_Query != nullptr)
				PdhCloseQuery(m_Query);
			m_Query = nullptr;
			if(m_pAdapter != nullptr)
				m_pAdapter->Release();
			m_pAdapter = nullptr;
		}

		float SampleGpuUtilPct()
		{
			EnsureGpuCountersInitialized();
			if(!m_GpuCountersAvailable || m_Query == nullptr)
				return -1.0f;
			if(!m_GpuCountersPrimed)
			{
				m_GpuCountersPrimed = PdhCollectQueryData(m_Query) == ERROR_SUCCESS;
				return -1.0f;
			}
			if(PdhCollectQueryData(m_Query) != ERROR_SUCCESS)
				return -1.0f;

			double TotalUtil = 0.0;
			bool AnyValue = false;
			for(PDH_HCOUNTER Counter : m_vCounters)
			{
				PDH_FMT_COUNTERVALUE Value = {};
				if(PdhGetFormattedCounterValue(Counter, PDH_FMT_DOUBLE, nullptr, &Value) == ERROR_SUCCESS && Value.CStatus == ERROR_SUCCESS)
				{
					TotalUtil += Value.doubleValue;
					AnyValue = true;
				}
			}
			return AnyValue ? std::clamp((float)TotalUtil, 0.0f, 100.0f) : -1.0f;
		}

		void SampleGpuMemory(SQmDevicePerfSample &Sample)
		{
			EnsureAdapterInitialized();
			if(m_pAdapter == nullptr)
				return;

			DXGI_QUERY_VIDEO_MEMORY_INFO LocalInfo = {};
			if(SUCCEEDED(m_pAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalInfo)))
			{
				Sample.m_GpuDedicatedVramMb = (float)LocalInfo.CurrentUsage / (1024.0f * 1024.0f);
				Sample.m_GpuDedicatedVramBudgetMb = (float)LocalInfo.Budget / (1024.0f * 1024.0f);
			}

			DXGI_QUERY_VIDEO_MEMORY_INFO NonLocalInfo = {};
			if(SUCCEEDED(m_pAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocalInfo)))
				Sample.m_GpuSharedVramMb = (float)NonLocalInfo.CurrentUsage / (1024.0f * 1024.0f);
		}

		SQmDevicePerfSample Sample()
		{
			SQmDevicePerfSample Sample;
			const uint64_t NowNs = (uint64_t)time_get_nanoseconds().count();

			Sample.m_GpuUtilPct = SampleGpuUtilPct();
			SampleGpuMemory(Sample);

			IO_COUNTERS IoCounters = {};
			if(GetProcessIoCounters(GetCurrentProcess(), &IoCounters))
			{
				Sample.m_DiskReadMbPerSec = QmComputeDiskReadMbPerSec(m_LastReadBytes, m_LastReadTickNs, IoCounters.ReadTransferCount, NowNs);
				m_LastReadBytes = IoCounters.ReadTransferCount;
				m_LastReadTickNs = NowNs;
			}

			Sample.m_Available =
				Sample.m_GpuUtilPct >= 0.0f ||
				Sample.m_GpuDedicatedVramMb >= 0.0f ||
				Sample.m_GpuDedicatedVramBudgetMb >= 0.0f ||
				Sample.m_GpuSharedVramMb >= 0.0f ||
				Sample.m_DiskReadMbPerSec >= 0.0f;
			return Sample;
		}
	};
#endif

} // namespace

SQmDevicePerfSnapshot CQmDevicePerfSnapshotCache::Publish(const SQmDevicePerfSample &Sample)
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_Snapshot.m_Sample = Sample;
	++m_Snapshot.m_Version;
	return m_Snapshot;
}

SQmDevicePerfSnapshot CQmDevicePerfSnapshotCache::Snapshot() const
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_Snapshot;
}

void CQmDevicePerfSnapshotCache::Reset()
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	m_Snapshot = {};
}

CQmAsyncDevicePerfSampler::CQmAsyncDevicePerfSampler(FSampleOverride SampleOverride, std::chrono::milliseconds PollInterval) :
	m_SampleOverride(std::move(SampleOverride)),
	m_PollInterval(PollInterval.count() > 0 ? PollInterval : std::chrono::milliseconds(1))
{
}

CQmAsyncDevicePerfSampler::~CQmAsyncDevicePerfSampler()
{
	Stop();
}

void CQmAsyncDevicePerfSampler::EnsureStarted()
{
	std::lock_guard<std::mutex> Lock(m_StateMutex);
	if(m_Started || m_StopRequested)
		return;
	m_Started = true;
	m_Thread = std::thread(&CQmAsyncDevicePerfSampler::Run, this);
}

void CQmAsyncDevicePerfSampler::SetEnabled(bool Enabled)
{
	{
		std::lock_guard<std::mutex> Lock(m_StateMutex);
		m_Enabled = Enabled;
	}
	m_StateCv.notify_all();
}

void CQmAsyncDevicePerfSampler::Stop()
{
	std::thread Thread;
	{
		std::lock_guard<std::mutex> Lock(m_StateMutex);
		if(!m_Started)
			return;
		if(m_StopRequested)
			return;
		m_StopRequested = true;
		m_Enabled = false;
		Thread = std::move(m_Thread);
	}
	m_StateCv.notify_all();
	if(Thread.joinable())
		Thread.join();
	{
		std::lock_guard<std::mutex> Lock(m_StateMutex);
		m_Cache.Reset();
		m_Started = false;
		m_StopRequested = false;
	}
}

SQmDevicePerfSnapshot CQmAsyncDevicePerfSampler::Snapshot() const
{
	return m_Cache.Snapshot();
}

void CQmAsyncDevicePerfSampler::Run()
{
#if defined(CONF_FAMILY_WINDOWS)
	CWindowsDevicePerfSource WindowsSource;
#endif

	auto SampleOnce = [&]() {
		if(m_SampleOverride)
			return m_SampleOverride();
#if defined(CONF_FAMILY_WINDOWS)
		return WindowsSource.Sample();
#else
		return SQmDevicePerfSample{};
#endif
	};

	std::unique_lock<std::mutex> Lock(m_StateMutex);
	while(!m_StopRequested)
	{
		if(!m_Enabled)
		{
			m_StateCv.wait(Lock, [&]() { return m_StopRequested || m_Enabled; });
			continue;
		}

		const std::chrono::milliseconds PollInterval = m_PollInterval;
		Lock.unlock();
		m_Cache.Publish(SampleOnce());
		Lock.lock();
		if(m_StopRequested)
			break;
		m_StateCv.wait_for(Lock, PollInterval, [&]() { return m_StopRequested || !m_Enabled; });
	}
}

void QmUpdateDevicePerfSamplerState(CQmAsyncDevicePerfSampler &Sampler, bool Enabled)
{
	if(!Enabled)
	{
		Sampler.SetEnabled(false);
		Sampler.Stop();
		return;
	}

	Sampler.EnsureStarted();
	Sampler.SetEnabled(true);
}
