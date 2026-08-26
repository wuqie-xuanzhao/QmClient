// QmClient 侧网易云 Hook 快照读取器。
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROVIDER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_PROVIDER_H

#include "qm_netease_hook_protocol.h"

#include <cstdint>
#include <memory>

class CQmNeteaseHookProvider
{
public:
	CQmNeteaseHookProvider();
	~CQmNeteaseHookProvider();

	CQmNeteaseHookProvider(const CQmNeteaseHookProvider &) = delete;
	CQmNeteaseHookProvider &operator=(const CQmNeteaseHookProvider &) = delete;

	void Start(const char *pHelperPath, int TimeoutMs);
	void Stop();
	bool Read(QmNeteaseHook::SSnapshot *pSnapshot, int TimeoutMs);
	bool IsRunning() const;

private:
	struct SImpl;
	std::unique_ptr<SImpl> m_pImpl;
};

#endif
