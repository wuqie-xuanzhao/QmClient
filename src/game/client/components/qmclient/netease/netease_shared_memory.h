#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_NETEASE_SHARED_MEMORY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_NETEASE_NETEASE_SHARED_MEMORY_H

#include <game/client/components/qmclient/netease_hook/qm_netease_hook_protocol.h>

#include <cstdint>
#include <memory>

namespace NeteaseLyrics
{
	// 对一份已经映射的 v5 block 做无锁一致性读取。函数不会把半写入 payload 暴露给调用方。
	bool ReadStableV5(const volatile QmNeteaseHook::SSharedBlockV5 &Shared, QmNeteaseHook::SSnapshotV5 *pOut, int MaxAttempts = 3);

	class CV5SharedMemoryReader
	{
	public:
		CV5SharedMemoryReader();
		~CV5SharedMemoryReader();
		CV5SharedMemoryReader(const CV5SharedMemoryReader &) = delete;
		CV5SharedMemoryReader &operator=(const CV5SharedMemoryReader &) = delete;

		bool Open();
		void Close();
		bool Read(QmNeteaseHook::SSnapshotV5 *pOut, uint64_t NowTick, uint64_t TimeoutMs);
		bool IsOpen() const;

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};

} // namespace NeteaseLyrics

namespace QmNetease
{
	using NeteaseLyrics::CV5SharedMemoryReader;
	using NeteaseLyrics::ReadStableV5;
}

#endif
