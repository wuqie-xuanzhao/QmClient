// QmClient 侧汽水音乐 Hook 快照读取器。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MUSIC_LYRICS_QM_SODA_HOOK_PROVIDER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MUSIC_LYRICS_QM_SODA_HOOK_PROVIDER_H

#include <qm-soda-hook/qm_soda_protocol.h>

#include <cstdint>
#include <memory>

// 读取汽水音乐 Helper 发布的共享内存快照,并负责启动/停止 helper 子进程。
class CQmSodaHookProvider
{
public:
	CQmSodaHookProvider();
	~CQmSodaHookProvider();

	CQmSodaHookProvider(const CQmSodaHookProvider &) = delete;
	CQmSodaHookProvider &operator=(const CQmSodaHookProvider &) = delete;

	void Start(const char *pHelperPath);
	void Stop();
	bool Read(QmSodaHook::SSnapshot *pSnapshot, int TimeoutMs);
	bool IsRunning() const;

private:
	struct SImpl;
	std::unique_ptr<SImpl> m_pImpl;
};

#endif
