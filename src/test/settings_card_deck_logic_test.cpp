#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/SettingsCardDeckLogic.h>

#include <gtest/gtest.h>

#include <array>

TEST(SettingsCardDeck, CrossColumnDropMovesOnlyTheGlobalModel)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());

	ASSERT_TRUE(CommitSettingsCardDeckDrop(Model, "graphics", "deck:graphics-display", 2, 0));
	const int Index = Model.FindByStableId("deck:graphics-display");
	ASSERT_GE(Index, 0);
	EXPECT_EQ(Model.Entry(Index).m_Column, 2);
	EXPECT_EQ(Model.Entry(Index).m_OrderInColumn, 0);
	EXPECT_STREQ(Model.Entry(Index).m_pDefaultTab, "graphics");
	EXPECT_TRUE(Model.IsDirty());
}

TEST(SettingsCardDeck, CrossColumnDropUsesVisibleOrderWhenHiddenCardsInterleave)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"sound-toggle", "sound", 1, 0},
		{"sound-hidden-before", "sound", 2, 0},
		{"sound-visible", "sound", 2, 1},
		{"sound-hidden-after", "sound", 2, 2},
	});
	const std::vector<int> vActiveStateIndices{
		Model.StateIndexForStableId("sound-toggle"),
		Model.StateIndexForStableId("sound-visible"),
	};

	// 拖拽只按当前可见卡片计数，隐藏卡片仍保留在持久化顺序中。
	ASSERT_TRUE(CommitSettingsCardDeckDrop(Model, "sound", "sound-toggle", 2, 1, &vActiveStateIndices));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"sound-hidden-before", "sound-visible", "sound-toggle", "sound-hidden-after"}));
}

TEST(SettingsCardDeck, EmptyVisibleColumnDropsBeforeHiddenCards)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"sound-toggle", "sound", 1, 0},
		{"sound-hidden", "sound", 2, 0},
	});
	const std::vector<int> vActiveStateIndices{Model.StateIndexForStableId("sound-toggle")};

	ASSERT_TRUE(CommitSettingsCardDeckDrop(Model, "sound", "sound-toggle", 2, 0, &vActiveStateIndices));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"sound-toggle", "sound-hidden"}));
}

TEST(SettingsCardDeck, SameColumnVisualNoOpPreservesHiddenRelativeOrder)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"sound-toggle", "sound", 2, 0},
		{"sound-hidden", "sound", 2, 1},
		{"sound-visible", "sound", 2, 2},
	});
	const std::vector<int> vActiveStateIndices{
		Model.StateIndexForStableId("sound-toggle"),
		Model.StateIndexForStableId("sound-visible"),
	};

	EXPECT_FALSE(CommitSettingsCardDeckDrop(Model, "sound", "sound-toggle", 2, 0, &vActiveStateIndices));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"sound-toggle", "sound-hidden", "sound-visible"}));
}

TEST(SettingsCardDeck, EdgeDragRequestsBoundedAutoScroll)
{
	const CUIRect Viewport{0.0f, 100.0f, 600.0f, 400.0f};
	EXPECT_LT(SettingsCardDeckAutoScrollDelta(101.0f, Viewport, 1.0f), 0.0f);
	EXPECT_GT(SettingsCardDeckAutoScrollDelta(499.0f, Viewport, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(SettingsCardDeckAutoScrollDelta(300.0f, Viewport, 1.0f), 0.0f);
}

TEST(SettingsCardDeck, CollapsedCardsSkipContentWorkAndExpandedDynamicCardsRemeasure)
{
	EXPECT_FALSE(SettingsCardDeckNeedsContentMeasure(true, false, -1.0f));
	EXPECT_FALSE(SettingsCardDeckRendersContent(true));
	EXPECT_TRUE(SettingsCardDeckNeedsContentMeasure(false, false, -1.0f));
	EXPECT_FALSE(SettingsCardDeckNeedsContentMeasure(false, false, 96.0f));
	EXPECT_TRUE(SettingsCardDeckNeedsContentMeasure(false, true, 96.0f));
	EXPECT_TRUE(SettingsCardDeckRendersContent(false));
}

TEST(SettingsCardDeck, ActiveCardMotionBlocksHeaderDragStart)
{
	EXPECT_TRUE(SettingsCardDeckAllowsDragStart(false, false, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(true, false, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, true, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, false, true, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, false, false, true));
}

TEST(SettingsCardDeck, DragPlacementUsesVisualOrderWithoutRendering)
{
	std::array<std::vector<int>, 3> aColumns{
		std::vector<int>{},
		std::vector<int>{4, 7},
		std::vector<int>{9},
	};
	ApplySettingsCardDeckDragPlacement(aColumns, 7, 2, 1);
	EXPECT_EQ(aColumns[1], (std::vector<int>{4}));
	EXPECT_EQ(aColumns[2], (std::vector<int>{9, 7}));

	// 意图：layout 已按每列视觉顺序收集 geometry，热路径无需再排序或分配。
	const std::vector<SSettingsCardDeckItemGeometry> vItems{
		{9, 2, {500.0f, 100.0f, 300.0f, 120.0f}},
		{7, 2, {500.0f, 168.0f, 300.0f, 52.0f}},
		{8, 2, {500.0f, 236.0f, 300.0f, 120.0f}},
	};
	EXPECT_EQ(ResolveSettingsCardDeckDropOrder(200.0f, 2, vItems, 7), 1);
}

TEST(SettingsCardDeck, SingleColumnDragPreservesCanonicalColumnCapacity)
{
	std::array<std::vector<int>, 3> aColumns{
		std::vector<int>{2},
		std::vector<int>{4, 7},
		std::vector<int>{9, 11},
	};
	ApplySettingsCardDeckSingleColumnDragPlacement(aColumns, 11, 1);
	EXPECT_EQ(aColumns[0], (std::vector<int>{2}));
	EXPECT_EQ(aColumns[1], (std::vector<int>{4, 11}));
	EXPECT_EQ(aColumns[2], (std::vector<int>{7, 9}));
}

TEST(SettingsCardDeck, SingleColumnDropRestoresDeterministicWideLayout)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"full", "sound", 0, 0},
		{"left-a", "sound", 1, 0},
		{"left-b", "sound", 1, 1},
		{"right-a", "sound", 2, 0},
		{"right-b", "sound", 2, 1},
	});
	const std::vector<int> vActiveStateIndices{
		Model.StateIndexForStableId("full"),
		Model.StateIndexForStableId("left-a"),
		Model.StateIndexForStableId("left-b"),
		Model.StateIndexForStableId("right-a"),
		Model.StateIndexForStableId("right-b"),
	};

	ASSERT_TRUE(CommitSettingsCardDeckSingleColumnDrop(Model, "sound", "right-b", 1, vActiveStateIndices));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 0), (std::vector<std::string>{"full"}));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 1), (std::vector<std::string>{"left-a", "right-b"}));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"left-b", "right-a"}));
}

TEST(SettingsCardDeck, SingleColumnDropPreservesHiddenCardsInCanonicalColumns)
{
	qm_card_order::CModel Model;
	Model.SetEntries({
		{"left-a", "sound", 1, 0},
		{"left-hidden", "sound", 1, 1},
		{"left-b", "sound", 1, 2},
		{"right-hidden", "sound", 2, 0},
		{"right-a", "sound", 2, 1},
		{"right-b", "sound", 2, 2},
	});
	const std::vector<int> vActiveStateIndices{
		Model.StateIndexForStableId("left-a"),
		Model.StateIndexForStableId("left-b"),
		Model.StateIndexForStableId("right-a"),
		Model.StateIndexForStableId("right-b"),
	};

	ASSERT_TRUE(CommitSettingsCardDeckSingleColumnDrop(Model, "sound", "right-b", 1, vActiveStateIndices));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 1), (std::vector<std::string>{"left-a", "left-hidden", "right-b"}));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"right-hidden", "left-b", "right-a"}));

	ASSERT_TRUE(CommitSettingsCardDeckSingleColumnDrop(Model, "sound", "left-a", 3, vActiveStateIndices));
	const std::vector<std::string> vExpectedLeft{"right-b", "left-hidden", "left-b"};
	const std::vector<std::string> vExpectedRight{"right-hidden", "right-a", "left-a"};
	EXPECT_EQ(Model.StableIdOrder("", "sound", 1), vExpectedLeft);
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), vExpectedRight);

	char aSerialized[1024];
	ASSERT_TRUE(Model.Serialize(aSerialized, sizeof(aSerialized)));
	qm_card_order::CModel Reloaded;
	ASSERT_TRUE(Reloaded.LoadExplicit(aSerialized, {
							       {"left-a", "sound", 1, 0},
							       {"left-hidden", "sound", 1, 1},
							       {"left-b", "sound", 1, 2},
							       {"right-hidden", "sound", 2, 0},
							       {"right-a", "sound", 2, 1},
							       {"right-b", "sound", 2, 2},
						       }));
	EXPECT_EQ(Reloaded.StableIdOrder("", "sound", 1), vExpectedLeft);
	EXPECT_EQ(Reloaded.StableIdOrder("", "sound", 2), vExpectedRight);
}

TEST(SettingsCardDeck, ColumnProjectionExcludesInactiveDefinitions)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	const std::vector<int> vActiveStateIndices{
		Model.StateIndexForStableId("deck:graphics-visual"),
		Model.StateIndexForStableId("deck:graphics-modes"),
	};
	const auto aColumns = BuildSettingsCardDeckColumnOrder(Model, "graphics", vActiveStateIndices);
	EXPECT_EQ(aColumns[0], (std::vector<int>{}));
	EXPECT_EQ(aColumns[1], (std::vector<int>{Model.StateIndexForStableId("deck:graphics-visual")}));
	EXPECT_EQ(aColumns[2], (std::vector<int>{Model.StateIndexForStableId("deck:graphics-modes")}));
}

TEST(SettingsCardDeck, ColumnProjectionCacheRebuildsOnlyForLayoutOrActiveDefinitionChanges)
{
	qm_card_order::CModel Model;
	Model.LoadMerged("", qm_card_registry::BuildDefaultEntries());
	const int Visual = Model.StateIndexForStableId("deck:graphics-visual");
	const int Modes = Model.StateIndexForStableId("deck:graphics-modes");
	const std::vector<int> vActiveStateIndices{Visual, Modes};
	settings_card_deck_logic::CProjectionCache Cache;

	const auto &aInitialColumns = Cache.Resolve(Model, "graphics", vActiveStateIndices);
	EXPECT_EQ(Cache.RebuildCount(), 1u);
	EXPECT_EQ(aInitialColumns[1], (std::vector<int>{Visual}));
	EXPECT_EQ(aInitialColumns[2], (std::vector<int>{Modes}));

	Cache.Resolve(Model, "graphics", vActiveStateIndices);
	EXPECT_EQ(Cache.RebuildCount(), 1u);

	const std::vector<int> vVisualOnly{Visual};
	const auto &aFilteredColumns = Cache.Resolve(Model, "graphics", vVisualOnly);
	EXPECT_EQ(Cache.RebuildCount(), 2u);
	EXPECT_EQ(aFilteredColumns[2], (std::vector<int>{}));

	ASSERT_TRUE(CommitSettingsCardDeckDrop(Model, "graphics", "deck:graphics-visual", 2, 0));
	const auto &aMovedColumns = Cache.Resolve(Model, "graphics", vActiveStateIndices);
	EXPECT_EQ(Cache.RebuildCount(), 3u);
	EXPECT_EQ(aMovedColumns[2], (std::vector<int>{Visual, Modes}));
}
