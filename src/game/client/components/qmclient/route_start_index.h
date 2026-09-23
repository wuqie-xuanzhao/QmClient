#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_ROUTE_START_INDEX_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_ROUTE_START_INDEX_H

#include <base/vmath.h>

#include <game/mapitems.h>

#include <limits>
#include <vector>

// 沿用距离场已有的递增地图扫描，避免路线显示每帧重新遍历全图找起点。
class CQmRouteStartIndex
{
	std::vector<int> m_vIndices;

public:
	void Reset() { m_vIndices.clear(); }

	// 每轮地图构建先 Reset，再按地图索引递增调用；游戏层与前景层同格只记录一次。
	bool AddTile(int Index, int GameTile, int FrontTile)
	{
		const bool IsStart = GameTile == TILE_START || FrontTile == TILE_START;
		if(IsStart)
			m_vIndices.push_back(Index);
		return IsStart;
	}

	template<typename E, typename P>
	int FindClosest(vec2 Position, int Fallback, E &&Eligible, P &&PositionOf) const
	{
		float BestDistanceSquared = std::numeric_limits<float>::max();
		int Closest = Fallback;
		for(const int Index : m_vIndices)
		{
			// 仍检查当前格子与可达性；缓存只记录潜在起点，不缓存最终选择。
			if(!Eligible(Index))
				continue;
			const float DistanceSquared = length_squared(Position - PositionOf(Index));
			if(DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				Closest = Index;
			}
		}
		return Closest;
	}
};

#endif
