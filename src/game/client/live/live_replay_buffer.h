// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_LIVE_LIVE_REPLAY_BUFFER_H
#define GAME_CLIENT_LIVE_LIVE_REPLAY_BUFFER_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

class CLiveReplayBuffer
{
public:
	class CSnapshotFrame
	{
	public:
		int m_Tick = -1;
		std::vector<uint8_t> m_vData;
	};

	void SetMaxFrames(std::size_t MaxFrames);
	void Clear();
	void PushSnapshot(int Tick, const void *pData, std::size_t DataSize);

	const std::deque<CSnapshotFrame> &Frames() const { return m_vFrames; }
	std::size_t MaxFrames() const { return m_MaxFrames; }

private:
	std::size_t m_MaxFrames = 0;
	std::deque<CSnapshotFrame> m_vFrames;
};

#endif // GAME_CLIENT_LIVE_LIVE_REPLAY_BUFFER_H
