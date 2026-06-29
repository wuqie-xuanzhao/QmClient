#ifndef GAME_CLIENT_QMUI_QMCARDREGISTRY_H
#define GAME_CLIENT_QMUI_QMCARDREGISTRY_H

#include <vector>

// 全局卡片组件注册表（单一事实源）。
// 每张卡的默认 Placement {stableId, tab, column, order}，是默认值、迁移兜底、新卡补位的唯一依据。
// tab 是可变位置维度（非卡片固有归属）；数据债卡（laser/nameplate_text）的 tab 在本表补齐。
// stableId 规范与 69 卡来源见 docs/superpowers/specs/2026-06-29-全局卡片-组件编辑器与stableId设计.md。
namespace qm_card_registry
{
	enum class ECardColumn
	{
		Left,
		Right,
		Full,
	};

	struct SCardDefault
	{
		const char *m_pStableId; // 全局唯一 stableId（qm:/tclient:/deck: 命名空间）
		const char *m_pDefaultTab; // 默认归属 tab（nullptr=横跨卡如 info，或暂无归属）
		ECardColumn m_DefaultColumn; // 默认列
		int m_DefaultOrder; // 列内默认序
	};

	// 全局卡片默认 Placement 表（栖梦 38 + Tclient 15 + deck 16 = 69）
	const std::vector<SCardDefault> &Defaults();

	// 按 stableId 查默认 Placement（未命中或 nullptr 返回 nullptr）
	const SCardDefault *FindByStableId(const char *pStableId);

	// 栖梦旧 key（持久化 key）→ stableId 迁移映射（从注册表派生，DRY）。
	// 命中返回 "qm:<key>"，未命中或 nullptr 返回 nullptr。
	// UI 名（如 QiaFen 的 keyword_reply）不在注册表故不映射——以持久化 key 为权威，避免迁移丢用户布局。
	const char *MigrateLegacyKey(const char *pLegacyKey);
} // namespace qm_card_registry

#endif // GAME_CLIENT_QMUI_QMCARDREGISTRY_H
