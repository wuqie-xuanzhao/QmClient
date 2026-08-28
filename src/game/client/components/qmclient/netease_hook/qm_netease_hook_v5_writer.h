#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_V5_WRITER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_HOOK_QM_NETEASE_HOOK_V5_WRITER_H

#include "qm_netease_hook_protocol.h"

#include <cstdint>
#include <memory>

namespace QmNeteaseHook
{
	// v5 共享内存 writer。只有拥有 mapping 的进程负责发布；客户端 reader
	// 永远不会写入 payload。该类仅用于 Windows helper/DLL，其他平台为空实现。
	class CV5Writer
	{
	public:
		CV5Writer();
		~CV5Writer();
		CV5Writer(const CV5Writer &) = delete;
		CV5Writer &operator=(const CV5Writer &) = delete;

		bool Open(bool PreferExisting = true);
		void Close();
		bool IsOpen() const;
		bool IsMappingOwner() const;
		bool Publish(SSnapshotV5 Snapshot);
		// 以同一把 writer mutex 原子检查当前来源并发布迁移期 fallback，
		// 防止 Helper 在检查和写入之间发布更高优先级结果后被覆盖。
		bool PublishFallback(SSnapshotV5 Snapshot, uint64_t NowTick, uint64_t TimeoutMs);
		bool Read(SSnapshotV5 *pSnapshot) const;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};
}

#endif
