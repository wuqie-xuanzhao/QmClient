#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_HOOK_COLL_VISIBILITY_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_HOOK_COLL_VISIBILITY_H

#include <base/vmath.h>

#include <game/mapitems.h>

#include <cstddef>

// 只跳过不可能进入当前视口的提示线，不裁剪真实钩子或预测。
class CQmHookCollVisibility
{
	bool m_HasHookTeleport = true;
	bool m_HasLegacyHookTeleport = true;

public:
	void OnMapLoad(const CTeleTile *pTiles, size_t NumTiles)
	{
		m_HasHookTeleport = false;
		m_HasLegacyHookTeleport = false;
		if(!pTiles)
			return;
		for(size_t Index = 0; Index < NumTiles; ++Index)
		{
			if(pTiles[Index].m_Number == 0)
				continue;
			m_HasHookTeleport |= pTiles[Index].m_Type == TILE_TELEINHOOK;
			m_HasLegacyHookTeleport |= pTiles[Index].m_Type == TILE_TELEIN;
			if(m_HasHookTeleport && m_HasLegacyHookTeleport)
				break;
		}
	}

	bool MayReachView(vec2 Position, float HookLength, float HookFireSpeed, vec2 ScreenMin, vec2 ScreenMax, float LinePadding, bool LegacyTeleport) const
	{
		// 钩子可传送到远处，不能只根据发起者与视口的距离剔除。
		if(LegacyTeleport ? m_HasLegacyHookTeleport : m_HasHookTeleport)
			return true;

		// 分量按 1/256 量化会略增向量长度；1 + 1/128 包住两轴舍入误差。
		// 额外覆盖末次发射步长、玩家圆交点、位置舍入及线宽，不只看角色是否在屏幕内。
		const float MaxDirectionLength = 1.0f + 1.0f / 128.0f;
		const float Reach = (HookLength + HookFireSpeed) * MaxDirectionLength + 100.0f + LinePadding;
		return !(Position.x + Reach < ScreenMin.x || Position.x - Reach > ScreenMax.x ||
			 Position.y + Reach < ScreenMin.y || Position.y - Reach > ScreenMax.y);
	}
};

#endif
