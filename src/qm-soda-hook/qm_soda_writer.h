#ifndef QM_SODA_HOOK_QM_SODA_WRITER_H
#define QM_SODA_HOOK_QM_SODA_WRITER_H

#include "qm_soda_protocol.h"

#include <memory>

// 汽水 Helper 侧的共享内存 writer:seqlock 发布播放态快照(仿网易云 v5 writer)。
namespace QmSodaHook
{
	class CSodaWriter
	{
	public:
		CSodaWriter();
		~CSodaWriter();
		CSodaWriter(const CSodaWriter &) = delete;
		CSodaWriter &operator=(const CSodaWriter &) = delete;

		bool Open(bool PreferExisting = true);
		void Close();
		bool IsOpen() const;
		bool Publish(SSnapshot Snapshot);

	private:
		struct SImpl;
		std::unique_ptr<SImpl> m_pImpl;
	};
} // namespace QmSodaHook

#endif
