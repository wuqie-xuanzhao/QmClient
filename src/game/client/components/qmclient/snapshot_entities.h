#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_SNAPSHOT_ENTITIES_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_SNAPSHOT_ENTITIES_H

#include <engine/client.h>

#include <generated/protocol.h>

#include <algorithm>
#include <cstddef>
#include <vector>

class CSnapEntities
{
public:
	IClient::CSnapItem m_Item;
	const CNetObj_EntityEx *m_pDataEx;
};

// 复用调用方容量，原地关联扩展信息；实体类型、排序和同 ID 关联规则沿用原实现。
inline void QmAttachSnapshotEntityExtensions(std::vector<CSnapEntities> &vEntities, std::vector<CSnapEntities> &vExtensions)
{
	const auto Compare = [](const CSnapEntities &Lhs, const CSnapEntities &Rhs) {
		return Lhs.m_Item.m_Id < Rhs.m_Item.m_Id;
	};
	std::sort(vEntities.begin(), vEntities.end(), Compare);
	std::sort(vExtensions.begin(), vExtensions.end(), Compare);

	size_t IndexEx = 0;
	for(CSnapEntities &Entity : vEntities)
	{
		while(IndexEx < vExtensions.size() && vExtensions[IndexEx].m_Item.m_Id < Entity.m_Item.m_Id)
			++IndexEx;
		Entity.m_pDataEx = nullptr;
		if(IndexEx < vExtensions.size() && vExtensions[IndexEx].m_Item.m_Id == Entity.m_Item.m_Id)
			Entity.m_pDataEx = static_cast<const CNetObj_EntityEx *>(vExtensions[IndexEx].m_Item.m_pData);
	}
	// 暂存区保留容量，但不跨快照保留借用的数据指针。
	vExtensions.clear();
}

#endif
