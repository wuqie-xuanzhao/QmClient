#include <game/client/QmUi/QmCardOrderModel.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <cstring>

// 意图：全局卡片顺序已经迁移到 stableId|tab|column|order 格式；
// 头文件注释必须同步说明旧冒号格式仅为兼容解析，避免后续实现继续按旧格式扩展。
TEST(QmCardOrderModel, HeaderDocumentsPipeFormatAndLegacyCompatibility)
{
	const std::string Header = ReadTestSourceFile("src/game/client/QmUi/QmCardOrderModel.h");

	EXPECT_NE(Header.find("格式 \"stableId|tab|column|order;\""), std::string::npos);
	EXPECT_NE(Header.find("兼容旧 \"id:col:order\""), std::string::npos);
	EXPECT_EQ(Header.find("格式 \"id:col:order;\""), std::string::npos);
}

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

TEST(QmCardOrderModel, MoveToTabChangesPlacementTabAndReordersTargetColumn)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:a", "visual", 1, 0}, {"qm:b", "hud", 1, 0}, {"qm:c", "hud", 1, 1}});
	M.ClearDirty();

	M.MoveToTab("qm:a", "hud", 1, 1);

	auto VisualLeft = M.ColumnIndices("visual", 1);
	EXPECT_TRUE(VisualLeft.empty());
	auto HudLeft = M.ColumnIndices("hud", 1);
	ASSERT_EQ(HudLeft.size(), 3u);
	EXPECT_STREQ(M.Entry(HudLeft[0]).m_pStableId, "qm:b");
	EXPECT_STREQ(M.Entry(HudLeft[1]).m_pStableId, "qm:a");
	EXPECT_STREQ(M.Entry(HudLeft[1]).m_pDefaultTab, "hud");
	EXPECT_STREQ(M.Entry(HudLeft[2]).m_pStableId, "qm:c");
	EXPECT_TRUE(M.IsDirty());
}

TEST(QmCardOrderModel, MoveToTabSerializesNewTabPlacement)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:a", "visual", 1, 0}, {"qm:b", "hud", 1, 0}});

	M.MoveToTab("qm:a", "hud", 2, 0);

	char aBuf[256];
	M.Serialize(aBuf, sizeof(aBuf));
	EXPECT_STREQ(aBuf, "qm:a|hud|right|0;qm:b|hud|left|0;");
}

TEST(QmCardOrderModel, SerializeParseRoundtrip)
{
	qm_card_order::CModel M;
	M.SetEntries({{"a", "visual", 1, 0}, {"b", "visual", 1, 1}, {"c", "hud", 2, 0}});
	char aBuf[256];
	M.Serialize(aBuf, sizeof(aBuf));
	EXPECT_STREQ(aBuf, "a|visual|left|0;b|visual|left|1;c|hud|right|0;");

	qm_card_order::CModel M2;
	std::vector<const char *> vValidIds = {"a", "b", "c"};
	ASSERT_TRUE(M2.Parse(aBuf, vValidIds));
	EXPECT_EQ(M2.Count(), 3);
	auto Col1 = M2.ColumnIndices("visual", 1);
	ASSERT_EQ(Col1.size(), 2u);
	EXPECT_STREQ(M2.Entry(Col1[0]).m_pStableId, "a");
	EXPECT_STREQ(M2.Entry(Col1[0]).m_pDefaultTab, "visual");
	EXPECT_STREQ(M2.Entry(Col1[1]).m_pStableId, "b");
	auto HudRight = M2.ColumnIndices("hud", 2);
	ASSERT_EQ(HudRight.size(), 1u);
	EXPECT_STREQ(M2.Entry(HudRight[0]).m_pStableId, "c");
	EXPECT_STREQ(M2.Entry(HudRight[0]).m_pDefaultTab, "hud");
}

// 意图：全局卡片配置依赖固定长度 config buffer；序列化必须暴露截断状态，
// 否则新增卡片后可能静默丢尾部 placement，下一次启动再把布局误当成用户配置。
TEST(QmCardOrderModel, SerializeReportsTruncation)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:first", "visual", 1, 0}, {"qm:second", "visual", 1, 1}});

	char aSmallBuf[20];
	EXPECT_FALSE(M.Serialize(aSmallBuf, sizeof(aSmallBuf)));

	char aFullBuf[128];
	EXPECT_TRUE(M.Serialize(aFullBuf, sizeof(aFullBuf)));
	EXPECT_STREQ(aFullBuf, "qm:first|visual|left|0;qm:second|visual|left|1;");
}

// 意图：全局卡片加载必须以注册表默认全量为基准，再用用户配置覆盖。
// 缺失卡补默认、未知/非法残留跳过，这是全局卡片唯一权威模型的核心兼容语义。
TEST(QmCardOrderModel, LoadMergedPreservesDefaultsAndAppliesValidUserPlacement)
{
	qm_card_order::CModel M;
	std::vector<qm_card_order::SEntry> Defaults = {
		{"qm:a", "visual", 1, 0},
		{"qm:b", "visual", 1, 1},
		{"qm:c", "hud", 2, 0},
	};

	EXPECT_TRUE(M.LoadMerged("qm:unknown|visual|left|0;qm:b|search|right|0;qm:c|hud|bad|4;", Defaults));

	EXPECT_EQ(M.Count(), 3);
	EXPECT_GE(M.StateIndexForStableId("qm:a"), 0);
	EXPECT_GE(M.StateIndexForStableId("qm:b"), 0);
	EXPECT_GE(M.StateIndexForStableId("qm:c"), 0);
	EXPECT_EQ(M.StateIndexForStableId("qm:unknown"), -1);

	const auto SearchRight = M.ColumnIndices("search", 2);
	ASSERT_EQ(SearchRight.size(), 1u);
	EXPECT_STREQ(M.Entry(SearchRight[0]).m_pStableId, "qm:b");
	EXPECT_EQ(M.Entry(SearchRight[0]).m_OrderInColumn, 0);

	const auto HudRight = M.ColumnIndices("hud", 2);
	ASSERT_EQ(HudRight.size(), 1u);
	EXPECT_STREQ(M.Entry(HudRight[0]).m_pStableId, "qm:c");
	EXPECT_EQ(M.Entry(HudRight[0]).m_OrderInColumn, 0);
}

// 意图：旧冒号格式没有 tab 字段；合并加载时必须继承默认 tab，
// 否则旧用户配置迁入全局模型后会从对应页面消失。
TEST(QmCardOrderModel, LoadMergedBackfillsDefaultTabForLegacyEntries)
{
	qm_card_order::CModel M;
	std::vector<qm_card_order::SEntry> Defaults = {
		{"a", "visual", 1, 0},
		{"b", "hud", 2, 0},
	};

	EXPECT_TRUE(M.LoadMerged("a:2:0", Defaults));

	const auto VisualRight = M.ColumnIndices("visual", 2);
	ASSERT_EQ(VisualRight.size(), 1u);
	EXPECT_STREQ(M.Entry(VisualRight[0]).m_pStableId, "a");
	EXPECT_STREQ(M.Entry(VisualRight[0]).m_pDefaultTab, "visual");
}

TEST(QmCardOrderModel, ParsePipeFormatKeepsMovableTabPlacement)
{
	qm_card_order::CModel M;
	std::vector<const char *> vValidIds = {"qm:chat_bubble", "tclient:visual-nameplates", "deck:graphics-display"};
	ASSERT_TRUE(M.Parse("qm:chat_bubble|search|left|0;tclient:visual-nameplates|tclient|right|0;deck:graphics-display|graphics|full|0;", vValidIds));
	EXPECT_EQ(M.Count(), 3);

	auto SearchLeft = M.ColumnIndices("search", 1);
	ASSERT_EQ(SearchLeft.size(), 1u);
	EXPECT_STREQ(M.Entry(SearchLeft[0]).m_pStableId, "qm:chat_bubble");
	EXPECT_STREQ(M.Entry(SearchLeft[0]).m_pDefaultTab, "search");

	auto TClientRight = M.ColumnIndices("tclient", 2);
	ASSERT_EQ(TClientRight.size(), 1u);
	EXPECT_STREQ(M.Entry(TClientRight[0]).m_pStableId, "tclient:visual-nameplates");
	EXPECT_STREQ(M.Entry(TClientRight[0]).m_pDefaultTab, "tclient");

	auto GraphicsFull = M.ColumnIndices("graphics", 0);
	ASSERT_EQ(GraphicsFull.size(), 1u);
	EXPECT_STREQ(M.Entry(GraphicsFull[0]).m_pStableId, "deck:graphics-display");
	EXPECT_STREQ(M.Entry(GraphicsFull[0]).m_pDefaultTab, "graphics");
}

TEST(QmCardOrderModel, ParsePipeFormatStillAcceptsLegacyNumericColumns)
{
	qm_card_order::CModel M;
	std::vector<const char *> vValidIds = {"qm:chat_bubble", "tclient:visual-nameplates"};
	ASSERT_TRUE(M.Parse("qm:chat_bubble|visual|1|0;tclient:visual-nameplates|tclient|2|0;", vValidIds));
	EXPECT_EQ(M.Count(), 2);

	auto VisualLeft = M.ColumnIndices("visual", 1);
	ASSERT_EQ(VisualLeft.size(), 1u);
	EXPECT_STREQ(M.Entry(VisualLeft[0]).m_pStableId, "qm:chat_bubble");

	auto TClientRight = M.ColumnIndices("tclient", 2);
	ASSERT_EQ(TClientRight.size(), 1u);
	EXPECT_STREQ(M.Entry(TClientRight[0]).m_pStableId, "tclient:visual-nameplates");
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

TEST(QmCardOrderModel, ParseSkipsNonNumericLegacyFields)
{
	qm_card_order::CModel M;
	std::vector<const char *> vValidIds = {"a", "b"};

	ASSERT_TRUE(M.Parse("a:x:0;a:1:nope;b:1:0", vValidIds));

	EXPECT_EQ(M.Count(), 1);
	auto Col1 = M.ColumnIndices(1);
	ASSERT_EQ(Col1.size(), 1u);
	EXPECT_STREQ(M.Entry(Col1[0]).m_pStableId, "b");
}

TEST(QmCardOrderModel, ParseSkipsNonNumericPipeFields)
{
	qm_card_order::CModel M;
	std::vector<const char *> vValidIds = {"qm:a", "qm:b"};

	ASSERT_TRUE(M.Parse("qm:a|visual|wat|0;qm:a|visual|left|nope;qm:b|visual|left|0", vValidIds));

	EXPECT_EQ(M.Count(), 1);
	auto VisualLeft = M.ColumnIndices("visual", 1);
	ASSERT_EQ(VisualLeft.size(), 1u);
	EXPECT_STREQ(M.Entry(VisualLeft[0]).m_pStableId, "qm:b");
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

TEST(QmCardOrderModel, NormalizeColumnsMarksDirtyWhenOrdersChange)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:a", "visual", 1, 5}, {"qm:b", "visual", 1, 9}});
	M.ClearDirty();

	M.NormalizeColumns();

	EXPECT_TRUE(M.IsDirty());
}

TEST(QmCardOrderModel, NormalizeColumnsKeepsIndependentTabColumns)
{
	qm_card_order::CModel M;
	M.SetEntries({{"qm:a", "visual", 1, 5}, {"qm:b", "hud", 1, 7}, {"qm:c", "visual", 1, 9}, {"qm:d", "hud", 2, 4}});

	M.NormalizeColumns();

	auto VisualLeft = M.ColumnIndices("visual", 1);
	ASSERT_EQ(VisualLeft.size(), 2u);
	EXPECT_STREQ(M.Entry(VisualLeft[0]).m_pStableId, "qm:a");
	EXPECT_EQ(M.Entry(VisualLeft[0]).m_OrderInColumn, 0);
	EXPECT_STREQ(M.Entry(VisualLeft[1]).m_pStableId, "qm:c");
	EXPECT_EQ(M.Entry(VisualLeft[1]).m_OrderInColumn, 1);

	auto HudLeft = M.ColumnIndices("hud", 1);
	ASSERT_EQ(HudLeft.size(), 1u);
	EXPECT_STREQ(M.Entry(HudLeft[0]).m_pStableId, "qm:b");
	EXPECT_EQ(M.Entry(HudLeft[0]).m_OrderInColumn, 0);

	auto HudRight = M.ColumnIndices("hud", 2);
	ASSERT_EQ(HudRight.size(), 1u);
	EXPECT_STREQ(M.Entry(HudRight[0]).m_pStableId, "qm:d");
	EXPECT_EQ(M.Entry(HudRight[0]).m_OrderInColumn, 0);
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
