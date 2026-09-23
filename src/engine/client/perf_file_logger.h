#ifndef ENGINE_CLIENT_PERF_FILE_LOGGER_H
#define ENGINE_CLIENT_PERF_FILE_LOGGER_H

#include <base/lock.h>
#include <base/logger.h>

#include <engine/shared/jobs.h>

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

// 关闭任务只持有日志对象，不借用客户端或 Storage；退出时也可同步接管未派发任务。
class CQmPerfLoggerCloseJob : public IJob
{
	std::mutex m_Mutex;
	std::shared_ptr<ILogger> m_pLogger;

protected:
	void Run() override { Close(); }

public:
	explicit CQmPerfLoggerCloseJob(std::shared_ptr<ILogger> pLogger) :
		m_pLogger(std::move(pLogger)) {}

	void Close()
	{
		const std::lock_guard<std::mutex> Lock(m_Mutex);
		m_pLogger.reset();
	}
};

class CQmPerfFileSwitchLogger : public ILogger
{
	CLock m_SwitchLock;
	std::shared_ptr<ILogger> m_pLogger;
	std::vector<std::shared_ptr<CQmPerfLoggerCloseJob>> m_vpClosing;

public:
	~CQmPerfFileSwitchLogger() override { FinishPending(); }

	void Set(std::shared_ptr<ILogger> pLogger)
	{
		{
			const CLockScope LockScope(m_SwitchLock);
			m_pLogger.swap(pLogger);
		}
		// 同步关闭仅用于初始化/退出，仍须在切换锁外析构。
	}

	std::shared_ptr<IJob> SetAsync(std::shared_ptr<ILogger> pLogger)
	{
		const CLockScope LockScope(m_SwitchLock);
		m_vpClosing.erase(std::remove_if(m_vpClosing.begin(), m_vpClosing.end(), [](const auto &pJob) { return pJob->Done(); }), m_vpClosing.end());
		auto pClose = std::make_shared<CQmPerfLoggerCloseJob>(std::move(m_pLogger));
		m_pLogger = std::move(pLogger);
		m_vpClosing.push_back(pClose);
		return pClose;
	}

	void FinishPending()
	{
		std::vector<std::shared_ptr<CQmPerfLoggerCloseJob>> vpClosing;
		{
			const CLockScope LockScope(m_SwitchLock);
			// 收尾期间仍登记任务，其他退出入口也必须等到同一次关闭完成。
			vpClosing = m_vpClosing;
		}
		for(const auto &pClose : vpClosing)
			pClose->Close();
	}

	void Log(const CLogMessage *pMessage) override
	{
		const CLockScope LockScope(m_SwitchLock);
		if(m_pLogger)
			m_pLogger->Log(pMessage);
	}

	void GlobalFinish() override
	{
		FinishPending();
		const CLockScope LockScope(m_SwitchLock);
		if(m_pLogger)
			m_pLogger->GlobalFinish();
	}

	void OnFilterChange() override
	{
		const CLockScope LockScope(m_SwitchLock);
		if(m_pLogger)
			m_pLogger->SetFilter(m_Filter);
	}
};

#endif
