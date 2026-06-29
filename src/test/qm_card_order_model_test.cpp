#include <game/client/QmUi/QmCardOrderModel.h>

#include <gtest/gtest.h>

#include <cstring>

TEST(QmCardOrderModel, MoveReordersWithinColumn)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", nullptr, 1, 0}, {"b", nullptr, 1, 1}, {"c", nullptr, 1, 2}});
	M.ClearDirty();
	M.Move("a", 1, 2); // a 移到末尾
	auto Col = M.ColumnIndices(1);
	ASSERT_EQ(Col.size(), 3u);
	EXPECT_STREQ(M.Entry(Col[0]).m_pStableId, "b");
	EXPECT_STREQ(M.Entry(Col[1]).m_pStableId, "c");
	EXPECT_STREQ(M.Entry(Col[2]).m_pStableId, "a");
	EXPECT_TRUE(M.IsDirty());
}

TEST(QmCardOrderModel, SerializeParseRoundtrip)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", nullptr, 1, 0}, {"b", nullptr, 1, 1}, {"c", nullptr, 2, 0}});
	char aBuf[256];
	M.Serialize(aBuf, sizeof(aBuf));

	qm_card_order::CModel M2;
	std::vector<const char *> vValidIds = {"a", "b", "c"};
	ASSERT_TRUE(M2.Parse(aBuf, vValidIds));
	EXPECT_EQ(M2.Count(), 3);
	auto Col1 = M2.ColumnIndices(1);
	ASSERT_EQ(Col1.size(), 2u);
	EXPECT_STREQ(M2.Entry(Col1[0]).m_pStableId, "a");
	EXPECT_STREQ(M2.Entry(Col1[1]).m_pStableId, "b");
}

TEST(QmCardOrderModel, ParseToleratesBadKeys)
{
	qm_card_order::CModel M;
	std::vector<const char *> vValidIds = {"a", "b"};
	// "x" 未知跳过，"a" 重复跳过，"b" column 非法(-1) 跳过
	ASSERT_TRUE(M.Parse("x:1:0;a:1:0;a:1:1;b:-1:0;b:1:0", vValidIds));
	EXPECT_EQ(M.Count(), 2); // a + b（合法）
	auto Col1 = M.ColumnIndices(1);
	ASSERT_EQ(Col1.size(), 2u);
	EXPECT_STREQ(M.Entry(Col1[0]).m_pStableId, "a");
	EXPECT_STREQ(M.Entry(Col1[1]).m_pStableId, "b");
}

TEST(QmCardOrderModel, NormalizeColumnsFillsGaps)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", nullptr, 1, 5}, {"b", nullptr, 1, 10}, {"c", nullptr, 2, 3}});
	M.ClearDirty();
	M.NormalizeColumns();
	auto Col1 = M.ColumnIndices(1);
	EXPECT_EQ(M.Entry(Col1[0]).m_OrderInColumn, 0);
	EXPECT_EQ(M.Entry(Col1[1]).m_OrderInColumn, 1);
	EXPECT_STREQ(M.Entry(Col1[0]).m_pStableId, "a");
	EXPECT_STREQ(M.Entry(Col1[1]).m_pStableId, "b");
	auto Col2 = M.ColumnIndices(2);
	ASSERT_EQ(Col2.size(), 1u);
	EXPECT_EQ(M.Entry(Col2[0]).m_OrderInColumn, 0);
}

TEST(QmCardOrderModel, MoveMarksDirty)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", nullptr, 1, 0}, {"b", nullptr, 1, 1}});
	M.ClearDirty();
	EXPECT_FALSE(M.IsDirty());
	M.Move("a", 1, 1);
	EXPECT_TRUE(M.IsDirty());
	M.ClearDirty();
	EXPECT_FALSE(M.IsDirty());
}

// 意图：组件编辑器按 tab+column 筛选本页卡片——这是"页面是展示层"的核心查询。
// tab 是可变位置维度（非卡片固有归属），column 是列，二者共同定位一张卡在画布上的位置。
TEST(QmCardOrderModel, ColumnIndicesFiltersByTabAndColumn)
{
	qm_card_order::CModel M;
	std::vector<qm_card_order::SEntry> E = {
		{"qm:chat_bubble", "visual", 1, 0}, // tab="visual", Left
		{"qm:coords", "hud", 1, 0}, // tab="hud", Left
		{"qm:camera_view", "visual", 2, 0}, // tab="visual", Right
	};
	M.SetEntries(E);
	// visual tab 的 Left 列：只应含 chat_bubble，不含 coords（hud）
	auto VisLeft = M.ColumnIndices("visual", 1);
	ASSERT_EQ(VisLeft.size(), 1u);
	EXPECT_STREQ(M.Entry(VisLeft[0]).m_pStableId, "qm:chat_bubble");
}

// 意图：让位 lerp 每帧每卡查 state index，必须 O(1)——否则 69 卡拖拽掉帧。
// 栖梦靠 EQmModuleId 连续枚举白嫖 O(1) state 下标；全局卡用 stableId 无连续枚举，
// 故建 stableId→连续 index 注册表，让位 lerp 保持 O(1) 查找（性能地基，禁线性退化 O(N²)）。
TEST(QmCardOrderModel, StateIndexIsOLookupByStableId)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:a", "t", 1, 0}, {"qm:b", "t", 1, 1}, {"qm:c", "t", 1, 2}});
	M.BuildStateIndex(); // 构建 stableId→连续 index
	// 已知 id O(1) 命中
	EXPECT_EQ(M.StateIndexForStableId("qm:b"), 1);
	// 未知 id 返回 -1（容错，不崩溃）
	EXPECT_EQ(M.StateIndexForStableId("qm:unknown"), -1);
}
