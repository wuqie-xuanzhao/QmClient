#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_HOOK_COLL_CANDIDATES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_HOOK_COLL_CANDIDATES_H

#include <engine/shared/protocol.h>

#include <array>
#include <vector>

// 仅用于钩子提示线：同一轮绘制的逐 tick 模拟共享可命中玩家名单。
class CQmHookCollCandidates
{
	std::array<std::vector<int>, MAX_CLIENTS> m_aCandidates;
	std::array<bool, MAX_CLIENTS> m_aReady = {};

public:
	void Reset() { m_aReady.fill(false); }
	template<typename F>
	const std::vector<int> &Get(int OwnId, F &&Eligible)
	{
		auto &vCandidates = m_aCandidates[OwnId];
		if(!m_aReady[OwnId])
		{
			vCandidates.clear();
			for(int Id = 0; Id < MAX_CLIENTS; ++Id)
			{
				if(Id != OwnId && Eligible(Id))
					vCandidates.push_back(Id);
			}
			m_aReady[OwnId] = true;
		}
		return vCandidates;
	}
};

#endif
