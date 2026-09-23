#ifndef BASE_PERF_TIMER_H
#define BASE_PERF_TIMER_H

#include <base/system.h>

#include <chrono>

class CPerfTimer
{
	std::chrono::nanoseconds m_Start;
	bool m_Enabled;

public:
	explicit CPerfTimer(bool Enabled = true) :
		m_Start(Enabled ? time_get_nanoseconds() : std::chrono::nanoseconds::zero()),
		m_Enabled(Enabled)
	{
	}

	void Reset()
	{
		if(m_Enabled)
			m_Start = time_get_nanoseconds();
	}

	double ElapsedMs() const
	{
		// 关闭诊断时不读取高精度时钟，默认构造仍保留原计时行为。
		if(!m_Enabled)
			return 0.0;
		return std::chrono::duration<double, std::milli>(time_get_nanoseconds() - m_Start).count();
	}
};

#endif // BASE_PERF_TIMER_H
