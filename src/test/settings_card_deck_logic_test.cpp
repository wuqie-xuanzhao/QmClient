#include <base/system.h>

#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/SettingsCardDeckLogic.h>

#include <gtest/gtest.h>

#include <array>

TEST(SettingsCardDeckLogic, MovesGraphicsAcrossColumnsWithoutOverwritingOtherDecks)
{
	settings_card_deck_logic::CLogic Logic;
	Logic.Load("graphics", "deck:graphics-display|sound|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;deck:ddnet-demo|ddnet|left|0;qm:chat_bubble|visual|left|0;");

	ASSERT_TRUE(Logic.Move("deck:graphics-visual", 2, 0));
	EXPECT_EQ(Logic.StableIdOrder(1), (std::vector<std::string>{"deck:graphics-display", "deck:graphics-backend"}));
	EXPECT_EQ(Logic.StableIdOrder(2), (std::vector<std::string>{"deck:graphics-visual", "deck:graphics-modes"}));
	EXPECT_FALSE(Logic.Move("deck:sound-toggle", 2, 0));
	EXPECT_FALSE(Logic.Move("deck:graphics-visual", 3, 0));

	char aSerialized[8192];
	ASSERT_TRUE(Logic.SerializeMerged("deck:graphics-display|sound|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;deck:ddnet-demo|ddnet|left|0;qm:chat_bubble|visual|left|0;", aSerialized, sizeof(aSerialized)));
	EXPECT_NE(str_find(aSerialized, "deck:graphics-display|graphics|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:graphics-visual|graphics|right|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:sound-toggle|sound|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:ddnet-demo|ddnet|left|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "qm:chat_bubble|visual|left|0"), nullptr);

	settings_card_deck_logic::CLogic Reloaded;
	Reloaded.Load("graphics", aSerialized);
	EXPECT_EQ(Reloaded.StableIdOrder(1), (std::vector<std::string>{"deck:graphics-display", "deck:graphics-backend"}));
	EXPECT_EQ(Reloaded.StableIdOrder(2), (std::vector<std::string>{"deck:graphics-visual", "deck:graphics-modes"}));
}

TEST(SettingsCardDeckLogic, SerializesCommittedProjection)
{
	char aGlobalOrder[8192] = "deck:graphics-display|graphics|left|0;deck:graphics-visual|graphics|left|1;deck:sound-toggle|sound|left|0;";
	settings_card_deck_logic::CLogic Logic;
	Logic.Load("graphics", aGlobalOrder);

	ASSERT_TRUE(Logic.Move("deck:graphics-visual", 2, 0));
	EXPECT_EQ(Logic.ColumnForStableId("deck:graphics-visual"), 2);
	char aSerialized[8192];
	ASSERT_TRUE(Logic.SerializeMerged(aGlobalOrder, aSerialized, sizeof(aSerialized)));
	EXPECT_NE(str_find(aSerialized, "deck:graphics-visual|graphics|right|0"), nullptr);
	EXPECT_NE(str_find(aSerialized, "deck:sound-toggle|sound|left|0"), nullptr);
	EXPECT_EQ(Logic.StableIdOrder(2), (std::vector<std::string>{"deck:graphics-visual", "deck:graphics-modes"}));
}

TEST(SettingsCardDeckLogic, CommitFailureKeepsProjectionConsistentWithGlobalOrder)
{
	const char *pGlobalOrder = "deck:graphics-display|graphics|left|0;deck:graphics-visual|graphics|left|1;";
	settings_card_deck_logic::CLogic Logic;
	Logic.Load("graphics", pGlobalOrder);

	ASSERT_TRUE(Logic.Move("deck:graphics-visual", 2, 0));
	char aSerialized[8];
	ASSERT_FALSE(Logic.SerializeMerged(pGlobalOrder, aSerialized, sizeof(aSerialized)));
	settings_card_deck_logic::CLogic Reloaded;
	Reloaded.Load("graphics", pGlobalOrder);
	EXPECT_EQ(Reloaded.ColumnForStableId("deck:graphics-visual"), 1);
	EXPECT_EQ(Reloaded.StableIdOrder(1), (std::vector<std::string>{"deck:graphics-display", "deck:graphics-visual", "deck:graphics-backend"}));
}
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
