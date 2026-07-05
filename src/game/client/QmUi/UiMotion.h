/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QMUI_UIMOTION_H
#define GAME_CLIENT_QMUI_UIMOTION_H

#include "QmAnimResolve.h"
#include "UiContext.h"

namespace ui_widget
{

	// 通用 widget 状态值补间：保留传入 transition 的完整运行时语义。
	inline float AnimateStateValue(const IUiContext &Ctx, const void *pId, EUiAnimProperty Property, float Target, const SUiAnimTransition &Transition)
	{
		const uint64_t NodeKey = BuildUiAnimNodeKey(Ctx.m_ScopeHash, reinterpret_cast<uint64_t>(pId));
		if(Ctx.m_pAnim == nullptr)
			return Target;
		return Ctx.m_pAnim->ResolveTargetValue(NodeKey, Property, Target, Transition);
	}

} // namespace ui_widget

#endif
