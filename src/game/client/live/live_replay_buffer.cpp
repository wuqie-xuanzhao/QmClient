// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "live_replay_buffer.h"

#include <base/system.h>

#include <utility>

void CLiveReplayBuffer::SetMaxFrames(std::size_t MaxFrames)
{
	m_MaxFrames = MaxFrames;
	while(m_vFrames.size() > m_MaxFrames)
		m_vFrames.pop_front();
}

void CLiveReplayBuffer::Clear()
{
	m_vFrames.clear();
}

void CLiveReplayBuffer::PushSnapshot(int Tick, const void *pData, std::size_t DataSize)
{
	if(m_MaxFrames == 0 || pData == nullptr || DataSize == 0)
		return;

	CSnapshotFrame Frame;
	Frame.m_Tick = Tick;
	Frame.m_vData.resize(DataSize);
	mem_copy(Frame.m_vData.data(), pData, DataSize);
	m_vFrames.push_back(std::move(Frame));

	while(m_vFrames.size() > m_MaxFrames)
		m_vFrames.pop_front();
}
