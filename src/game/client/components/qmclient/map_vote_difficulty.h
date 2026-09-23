#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MAP_VOTE_DIFFICULTY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MAP_VOTE_DIFFICULTY_H

#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>

#include <game/voting.h>

#include <algorithm>
#include <cstdint>
#include <vector>

// 只缓存菜单展示难度，不改变投票选项或投票行为。
class CQmMapVoteDifficulty
{
	struct SEntry
	{
		char m_aMapName[VOTE_DESC_LENGTH];
		int m_Stars;
		int m_Order;
	};
	std::vector<SEntry> m_vEntries;
	uint64_t m_Revision = 0;
	bool m_Valid = false;

	void Rebuild(uint64_t Revision, const CVoteOptionClient *pFirst)
	{
		m_vEntries.clear();
		int Order = 0;
		for(const CVoteOptionClient *pOption = pFirst; pOption; pOption = pOption->m_pNext, ++Order)
		{
			const char *pDescription = pOption->m_aDescription;
			const char *pBy = str_find_nocase(pDescription, " by ");
			const char *pStars = str_find(pDescription, "/5");
			if(!pBy || pBy == pDescription || !pStars || pStars <= pDescription)
				continue;
			const char *pStarNumber = pStars;
			while(pStarNumber > pDescription && pStarNumber[-1] >= '0' && pStarNumber[-1] <= '9')
				--pStarNumber;
			if(pStarNumber == pStars)
				continue;
			char aStars[8];
			const int StarNumberLength = minimum((int)(pStars - pStarNumber), (int)sizeof(aStars) - 1);
			str_copy(aStars, pStarNumber, StarNumberLength + 1);
			const int Stars = str_toint(aStars);
			if(Stars < 0 || Stars > 5)
				continue;
			SEntry Entry{};
			const int MapNameLength = (int)(pBy - pDescription);
			mem_copy(Entry.m_aMapName, pDescription, MapNameLength);
			Entry.m_Stars = Stars;
			Entry.m_Order = Order;
			m_vEntries.push_back(Entry);
		}
		// 同名项按链表顺序排列，二分查找仍命中第一条有效描述。
		std::sort(m_vEntries.begin(), m_vEntries.end(), [](const SEntry &A, const SEntry &B) {
			const int Compare = str_comp_nocase(A.m_aMapName, B.m_aMapName);
			return Compare != 0 ? Compare < 0 : A.m_Order < B.m_Order;
		});
		m_Revision = Revision;
		m_Valid = true;
	}

public:
	int Find(uint64_t Revision, const CVoteOptionClient *pFirst, const char *pMapName)
	{
		if(!m_Valid || m_Revision != Revision)
			Rebuild(Revision, pFirst);
		if(!pMapName || pMapName[0] == '\0')
			return -1;
		const auto It = std::lower_bound(m_vEntries.begin(), m_vEntries.end(), pMapName, [](const SEntry &Entry, const char *pName) {
			return str_comp_nocase(Entry.m_aMapName, pName) < 0;
		});
		return It != m_vEntries.end() && str_comp_nocase(It->m_aMapName, pMapName) == 0 ? It->m_Stars : -1;
	}
};

#endif
