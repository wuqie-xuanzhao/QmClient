#include <game/client/QmUi/QmCardOrderModel.h>

#include <gtest/gtest.h>

#include <cstring>

TEST(QmCardOrderModel, MoveReordersWithinColumn)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", 1, 0}, {"b", 1, 1}, {"c", 1, 2}});
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
	M.SetEntries({{"a", 1, 0}, {"b", 1, 1}, {"c", 2, 0}});
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
	M.SetEntries({{"a", 1, 5}, {"b", 1, 10}, {"c", 2, 3}});
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
	M.SetEntries({{"a", 1, 0}, {"b", 1, 1}});
	M.ClearDirty();
	EXPECT_FALSE(M.IsDirty());
	M.Move("a", 1, 1);
	EXPECT_TRUE(M.IsDirty());
	M.ClearDirty();
	EXPECT_FALSE(M.IsDirty());
}
