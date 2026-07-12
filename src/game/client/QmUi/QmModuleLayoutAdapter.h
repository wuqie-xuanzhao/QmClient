#ifndef GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
#define GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H

#include <game/client/QmUi/QmCardOrderModel.h>
#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/QmModuleTypes.h>

#include <span>
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

	// 栖梦折叠状态兼容历史 key[:...];key 格式，写回时仅保留已注册 key 并规范化为 key;key。
	// pEntries 的顺序与 pCollapsed 的索引必须一致；未知和重复条目均忽略。
	bool ParseLegacyQmCollapsed(const char *pConfig, std::span<const SQmModuleEntry> Entries, std::span<bool> Collapsed);
	void SerializeLegacyQmCollapsed(std::span<const SQmModuleEntry> Entries, std::span<const bool> Collapsed, char *pOut, int OutSize);

	// === CModel 接入（Step 2）：栖梦布局由 qm_card_order::CModel 持有，IsDirty 触发序列化 ===
	// 栖梦布局的 CModel 单例（B1 Task 5 的布局权威，替代 s_aQmModuleLayout 的序列化职责）。
	qm_card_order::CModel &QmModuleLayoutModel();
	// 调用方拥有的全局模型导入入口：只覆盖 qm:* 或 tclient:* 的 legacy placement，不创建平行 model。
	bool LoadLegacyQmLayoutIntoModel(qm_card_order::CModel &Model, const char *pConfig);
	bool LoadLegacyTClientLayoutIntoModel(qm_card_order::CModel &Model, const char *pConfig);
	bool MoveQmModuleInModel(qm_card_order::CModel &Model, EQmModuleId Id, EQmModuleColumn TargetColumn, int TargetOrder);
	bool MoveQmModuleToTabInModel(qm_card_order::CModel &Model, EQmModuleId Id, const char *pTargetTab, EQmModuleColumn TargetColumn, int TargetOrder);
	bool SerializeLegacyQmLayoutFromModel(const qm_card_order::CModel &Model, char *pOut, int OutSize);
	// 栖梦 config → 过渡 singleton CModel：P6 前旧 renderer 保留该 wrapper。
	void LoadQmLayoutIntoModel(const char *pConfig, const std::vector<SQmModuleEntry> &vDefaults);
	bool LoadQmLayoutModelFromGlobalOrder(const char *pConfig, const std::vector<SQmModuleEntry> &vDefaults);
	// CModel → SQmModuleEntry[]（按 stableId 反查 EQmModuleId，column int→枚举，key=stableId 去 "qm:" 前缀 = 持久化 key）。
	// 供 Step 4 的 RefreshQmModuleLayoutFromModel（写 s_aQmModuleLayout）+ SerializeQmLayoutFromModel 复用。
	std::vector<SQmModuleEntry> SyncModelToLegacyLayout();
	// CModel → 栖梦 config：CModel entries 转 SQmModuleEntry（key 用 stableId 去 "qm:" 前缀 = 持久化 key），SerializeLegacyQmLayout 输出。
	void SerializeQmLayoutFromModel(char *pOut, int OutSize);
	// CModel → 全局 config：用当前 Qm 子模型更新 qm:* 条目，同时保留已有 tclient:/deck: 等非 Qm 条目。
	bool SerializeMergedGlobalCardOrderFromQmModel(const char *pExistingGlobalOrder, char *pOut, int OutSize);
	bool MigrateQmLayoutToGlobalCardOrder(const std::vector<SQmModuleEntry> &vDefaults);
	// 过渡 singleton wrapper：P6 前旧 renderer 使用；新路径必须传入调用方拥有的 model。
	bool MoveQmModuleInModel(EQmModuleId Id, EQmModuleColumn TargetColumn, int TargetOrder);
	bool MoveQmModuleToTabInModel(EQmModuleId Id, const char *pTargetTab, EQmModuleColumn TargetColumn, int TargetOrder);
	bool IsQmLayoutModelDirty();
	void ClearQmLayoutModelDirty();
} // namespace qm_module

#endif // GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
