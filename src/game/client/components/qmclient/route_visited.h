#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_ROUTE_VISITED_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_ROUTE_VISITED_H

#include <cstddef>
#include <cstdint>
#include <vector>

// 路线显示只清理上一条路径触及的位图字，避免每帧分配并清零整张地图。
class CQmRouteVisited
{
	std::vector<uint64_t> m_vWords;
	std::vector<size_t> m_vDirtyWords;

public:
	void Begin(size_t NumTiles)
	{
		const size_t NumWords = (NumTiles + 63) / 64;
		if(m_vWords.size() != NumWords)
			m_vWords.assign(NumWords, 0);
		else
			for(const size_t Index : m_vDirtyWords)
				m_vWords[Index] = 0;
		m_vDirtyWords.clear();
	}

	// 调用方先验证地图索引；返回 false 表示路径已经访问此格。
	bool Visit(size_t Index)
	{
		const size_t WordIndex = Index / 64;
		const uint64_t Mask = uint64_t(1) << (Index % 64);
		uint64_t &Word = m_vWords[WordIndex];
		if(Word & Mask)
			return false;
		if(Word == 0)
			m_vDirtyWords.push_back(WordIndex);
		Word |= Mask;
		return true;
	}

	void Reset()
	{
		m_vWords.clear();
		m_vDirtyWords.clear();
	}
};

#endif
