#ifndef GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
#define GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H

#include <game/client/QmUi/QmModuleTypes.h>

// 栖梦侧栏布局适配层（B1 Task 5）。
// EQmModuleId/EQmModuleColumn（栖梦枚举）↔ stableId/int（qm_card_order::CModel）双向转换，
// 供 SyncQmModuleLayout 切到 CModel。列编码：Full=0, Left=1, Right=2
//（与栖梦 ParseQmModuleColumn 整数分支一致）。
namespace qm_module
{
	int QmModuleColumnToInt(EQmModuleColumn Column);
	EQmModuleColumn QmModuleColumnFromInt(int Column);
	const char *QmModuleColumnToString(EQmModuleColumn Column); // "full"/"left"/"right"
	bool ParseQmModuleColumnString(const char *pStr, EQmModuleColumn *pOut);

	// EQmModuleId ↔ stableId（"qm:"+持久化 key）。
	// QiaFen 以持久化 key qiafen 为权威（非 UI 名 keyword_reply），与 s_aQmModuleDefaults.m_pKey 一致。
	const char *QmModuleStableId(EQmModuleId Id);
	bool QmModuleIdFromStableId(const char *pStableId, EQmModuleId *pOut); // 未命中返回 false
} // namespace qm_module

#endif // GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
