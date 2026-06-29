#ifndef GAME_CLIENT_QMUI_QMCARDORDERMODEL_H
#define GAME_CLIENT_QMUI_QMCARDORDERMODEL_H

#include <string>
#include <unordered_map>
#include <vector>

// 卡片顺序数据模型（阶段 2 拖拽架构的地基）。
// 唯一权威的卡片顺序数据源，替代当前散落的 s_aQmModuleLayout / m_vTClientLeftCardOrder /
// m_SettingsCardDeckOrders 三套并存。以栖梦 ParseQmModuleLayout / SerializeQmModuleLayout /
// NormalizeQmModuleLayoutColumns 为骨架演进。
namespace qm_card_order
{
	struct SEntry
	{
		const char *m_pStableId = nullptr; // 稳定 id（栖梦用 m_pKey 如 "chat_bubble"，Tclient 用 stable id）
		const char *m_pDefaultTab = nullptr; // 该卡当前归属 tab（tab 是可变位置维度而非固有归属；运行时为注册表默认 tab 的快照）
		int m_Column = 0; // 列号（0=Full/Left, 1=Right，或泛化）
		int m_OrderInColumn = 0; // 列内排序
	};

	class CModel
	{
	public:
		void SetEntries(std::vector<SEntry> Entries);
		int Count() const { return (int)m_vEntries.size(); }
		const SEntry &Entry(int Index) const { return m_vEntries[Index]; }

		int FindByStableId(const char *pStableId) const;

		// 核心操作：把 pStableId 移到 ToColumn 的 ToOrder 位置
		void Move(const char *pStableId, int ToColumn, int ToOrder);

		// order 连续化（消除空洞），按列 stable_sort
		void NormalizeColumns();

		// 按序返回某列的 entry index
		std::vector<int> ColumnIndices(int Column) const;

		// 按序返回某 tab 某 column 的 entry index（组件编辑器按"页是展示层"筛选：tab=页/画布，column=列）
		std::vector<int> ColumnIndices(const char *pTab, int Column) const;

		// 构建 stableId→连续 index 注册表（让位 lerp 保持 O(1) 查找的性能地基；SetEntries/Parse 后自动维护）
		void BuildStateIndex();
		// O(1) 查询 stableId 的 state index（未命中或 nullptr 返回 -1）
		int StateIndexForStableId(const char *pStableId) const;

		// 持久化：格式 "id:col:order;"（照搬栖梦 QmSidebarCardOrder 格式）
		void Serialize(char *pBuf, int BufSize) const;
		// 容错解析：未知/重复/非法 key 跳过（照搬 ParseQmModuleLayout）
		bool Parse(const char *pStr, const std::vector<const char *> &vValidIds);

		bool IsDirty() const { return m_Dirty; }
		void ClearDirty() { m_Dirty = false; }

	private:
		std::vector<SEntry> m_vEntries;
		std::unordered_map<std::string, int> m_StableIdToState; // stableId→连续 index 注册表（让位 lerp O(1) 查找）
		bool m_Dirty = false;
	};
} // namespace qm_card_order

#endif // GAME_CLIENT_QMUI_QMCARDORDERMODEL_H
