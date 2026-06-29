#ifndef GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
#define GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H

#include <game/client/QmUi/QmModuleTypes.h>

#include <vector>

namespace qm_module
{
	// 列编码 Full=0/Left=1/Right=2（与栖梦 ParseQmModuleColumn 整数分支一致）。
	int QmModuleColumnToInt(EQmModuleColumn Column);
	EQmModuleColumn QmModuleColumnFromInt(int Column);
	const char *QmModuleColumnToString(EQmModuleColumn Column); // "full"/"left"/"right"
	bool ParseQmModuleColumnString(const char *pStr, EQmModuleColumn *pOut);

	// EQmModuleId ↔ stableId（"qm:"+持久化 key）。
	// QiaFen 以持久化 key qiafen 为权威（非 UI 名 keyword_reply），与 s_aQmModuleDefaults.m_pKey 一致。
	const char *QmModuleStableId(EQmModuleId Id);
	bool QmModuleIdFromStableId(const char *pStableId, EQmModuleId *pOut); // 未命中返回 false

	// 栖梦布局解析/序列化（复用栖梦 ParseQmModuleLayout 算法：defaults 基准 + Full 保护 + 容错 + Normalize）。
	// vDefaults 提供合法 key 白名单 + 默认 placement（Full 保护基准）；vOut 初始化为 vDefaults 副本后用解析值覆盖。
	// 容错：未知/重复 key 跳过；非法 column/order 字段跳过整条；空 config → defaults + Normalize。
	void ParseLegacyQmLayout(const char *pConfig, const std::vector<SQmModuleEntry> &vDefaults, std::vector<SQmModuleEntry> &vOut);
	// 输出栖梦格式 "key:column:order;"（column 字符串 full/left/right），条目间用 ';' 分隔。
	void SerializeLegacyQmLayout(const std::vector<SQmModuleEntry> &vEntries, char *pOut, int OutSize);
	// Left/Right 列内 order 连续化（消除空洞），按 OrderInColumn stable_sort。
	void NormalizeQmLayoutColumns(std::vector<SQmModuleEntry> &vEntries);
} // namespace qm_module

#endif // GAME_CLIENT_QMUI_QMMODULELAYOUTADAPTER_H
