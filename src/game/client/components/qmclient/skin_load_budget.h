#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_LOAD_BUDGET_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SKIN_LOAD_BUDGET_H

#include <chrono>
#include <utility>

inline bool QmSkinCanFinalize(int Processed, std::chrono::nanoseconds Elapsed, std::chrono::nanoseconds Budget)
{
	// 首个已就绪皮肤始终取得进展，后续皮肤按时间预算留到下一轮处理。
	return Processed == 0 || Elapsed < Budget;
}

class CQmSkinUploadFrameBudget
{
public:
	// 多次逻辑更新共享一次上传额度，只在实际渲染或启动预热时重置。
	bool TryConsume() { return !std::exchange(m_Consumed, true); }
	void Reset() { m_Consumed = false; }

private:
	bool m_Consumed = false;
};

#endif
