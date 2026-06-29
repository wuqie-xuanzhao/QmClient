#ifndef GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
#define GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H

#include <game/client/QmUi/QmCardOrderModel.h>
#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/QmModuleTypes.h>

#include <vector>

namespace qm_module
{
	// 列编码 Full=0/Left=1/Right=2（与栖梦 ParseQmModuleColumn 整数分支一致）。
	int QmModuleColumnToInt(EQmModuleColumn Column);
	EQmModuleColumn QmModuleColumnFromInt(int Column);
	const char *QmModuleColumnToString(EQmModuleColumn Column); // "full"/"left"/"right"
	// 等价栖梦 ParseQmModuleColumn：str_comp_nocase 命中 full/left/right，否则 str_toint 解析 0/1/2。
	bool ParseQmModuleColumnString(const char *pStr, EQmModuleColumn *pOut);

	// EQmModuleId ↔ stableId（"qm:"+持久化 key）。
	// QiaFen 以持久化 key qiafen 为权威（非 UI 名 keyword_reply），与 s_aQmModuleDefaults.m_pKey 一致。
	const char *QmModuleStableId(EQmModuleId Id);
	bool QmModuleIdFromStableId(const char *pStableId, EQmModuleId *pOut); // 未命中返回 false

	// 栖梦布局解析/序列化（复用栖梦 ParseQmModuleLayout 算法：defaults 基准 + Full 保护 + 容错 + Normalize）。
	// vDefaults 提供合法 key 白名单 + 默认 placement（Full 保护基准）；vOut 初始化为 vDefaults 副本后用解析值覆盖。
	// 返回是否至少解析到一条（false=空 config 或全未知 key）；SmartDefaults 均衡由调用方负责（栖梦 SyncQmModuleLayout 的事）。
	bool ParseLegacyQmLayout(const char *pConfig, const std::vector<SQmModuleEntry> &vDefaults, std::vector<SQmModuleEntry> &vOut);
	void SerializeLegacyQmLayout(const std::vector<SQmModuleEntry> &vEntries, char *pOut, int OutSize);
	void NormalizeQmLayoutColumns(std::vector<SQmModuleEntry> &vEntries);

	// === CModel 接入（Step 2）：栖梦布局由 qm_card_order::CModel 持有，IsDirty 触发序列化 ===
	// 栖梦布局的 CModel 单例（B1 Task 5 的布局权威，替代 s_aQmModuleLayout 的序列化职责）。
	qm_card_order::CModel &QmModuleLayoutModel();
	// 栖梦 config → CModel：ParseLegacyQmLayout 解析（defaults 基准 + Full 保护 + 容错 + Normalize）
	// → 转 CModel::SEntry（stableId + tab 从注册表查 + column int + order）→ SetEntries；加载后 ClearDirty。
	void LoadQmLayoutIntoModel(const char *pConfig, const std::vector<SQmModuleEntry> &vDefaults);
	// CModel → SQmModuleEntry[]（按 stableId 反查 EQmModuleId，column int→枚举，key=stableId 去 "qm:" 前缀 = 持久化 key）。
	// 供 Step 4 的 RefreshQmModuleLayoutFromModel（写 s_aQmModuleLayout）+ SerializeQmLayoutFromModel 复用。
	std::vector<SQmModuleEntry> SyncModelToLegacyLayout();
	// CModel → 栖梦 config：CModel entries 转 SQmModuleEntry（key 用 stableId 去 "qm:" 前缀 = 持久化 key），SerializeLegacyQmLayout 输出。
	void SerializeQmLayoutFromModel(char *pOut, int OutSize);
	// CModel.Move + Full 保护（目标 Full 拒绝——非 Full 卡不可拖成 Full；源 Full 卡的拒拖由调用方 CommitDropPreview 保证）。返回是否移动。
	bool MoveQmModuleInModel(EQmModuleId Id, EQmModuleColumn TargetColumn, int TargetOrder);
	bool IsQmLayoutModelDirty();
	void ClearQmLayoutModelDirty();
} // namespace qm_module

#endif // GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
