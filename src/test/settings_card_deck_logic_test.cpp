#include <game/client/QmUi/QmCardRegistry.h>
#include <game/client/QmUi/SettingsCard.h>
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

TEST(SettingsCardDeck, CollapseAndVisibilityChangesSnapWithoutDisablingDragReflow)
{
	EXPECT_FALSE(SettingsCardDeckContentHeightChanged(-1.0f, 96.0f));
	EXPECT_FALSE(SettingsCardDeckContentHeightChanged(96.0f, 96.005f));
	EXPECT_TRUE(SettingsCardDeckContentHeightChanged(96.0f, 120.0f));
	EXPECT_TRUE(SettingsCardDeckShouldSnapReflow(true, false));
	EXPECT_FALSE(SettingsCardDeckShouldSnapReflow(false, false));
	EXPECT_FALSE(SettingsCardDeckShouldSnapReflow(true, true));
}

TEST(SettingsCardDeck, SubtitleVisibilityUsesCurrentPointerMotionLatchAndFocus)
{
	EXPECT_TRUE(SettingsCardSubtitleVisible(true, false, false));
	EXPECT_TRUE(SettingsCardSubtitleVisible(false, true, false));
	EXPECT_TRUE(SettingsCardSubtitleVisible(false, false, true));
	EXPECT_FALSE(SettingsCardSubtitleVisible(false, false, false));
}

TEST(SettingsCardDeck, SubtitleVisibilityLatchesOnlyWhileCardIsMoving)
{
	EXPECT_TRUE(ResolveSettingsCardSubtitleMotionLatch(true, true, false, false));
	EXPECT_TRUE(ResolveSettingsCardSubtitleMotionLatch(false, true, true, true));
	EXPECT_FALSE(ResolveSettingsCardSubtitleMotionLatch(false, true, true, false));
	EXPECT_FALSE(ResolveSettingsCardSubtitleMotionLatch(false, false, true, true));
}

TEST(SettingsCardDeck, GeometryMotionIncludesCardsPushedByAnEarlierHeightAnimation)
{
	EXPECT_FALSE(SettingsCardDeckGeometryMoved(false, 100.0f, 80.0f, 120.0f, 80.0f));
	EXPECT_FALSE(SettingsCardDeckGeometryMoved(true, 100.0f, 80.0f, 100.0f, 80.0f));
	EXPECT_TRUE(SettingsCardDeckGeometryMoved(true, 100.0f, 80.0f, 120.0f, 80.0f));
	EXPECT_TRUE(SettingsCardDeckGeometryMoved(true, 100.0f, 80.0f, 100.0f, 90.0f));
}

TEST(SettingsCardDeck, RestingCardsDoNotDrawASecondRoundedBorder)
{
	SSettingsCardVisualState State;
	EXPECT_FALSE(SettingsCardInteractionBorderVisible(State));
	State.m_Hovered = true;
	EXPECT_TRUE(SettingsCardInteractionBorderVisible(State));
	State.m_Hovered = false;
	State.m_Focused = true;
	EXPECT_TRUE(SettingsCardInteractionBorderVisible(State));
	State.m_Focused = false;
	State.m_DropFeedback = true;
	EXPECT_TRUE(SettingsCardInteractionBorderVisible(State));
}

TEST(SettingsCardDeck, CardSurfaceColorIgnoresBorderInteractionState)
{
	const ColorRGBA BaseSurface(0.12f, 0.24f, 0.36f, 0.48f);
	SSettingsCardVisualState Resting;
	Resting.m_DrawAlpha = 0.75f;
	SSettingsCardVisualState Interactive = Resting;
	Interactive.m_Hovered = true;
	Interactive.m_Focused = true;
	Interactive.m_DropFeedback = true;

	const ColorRGBA RestingSurface = ResolveSettingsCardSurfaceColor(BaseSurface, Resting);
	const ColorRGBA InteractiveSurface = ResolveSettingsCardSurfaceColor(BaseSurface, Interactive);
	EXPECT_FLOAT_EQ(RestingSurface.r, InteractiveSurface.r);
	EXPECT_FLOAT_EQ(RestingSurface.g, InteractiveSurface.g);
	EXPECT_FLOAT_EQ(RestingSurface.b, InteractiveSurface.b);
	EXPECT_FLOAT_EQ(RestingSurface.a, InteractiveSurface.a);
	EXPECT_FLOAT_EQ(RestingSurface.a, BaseSurface.a * Resting.m_DrawAlpha);
}

TEST(SettingsCardDeck, EveryRegisteredCardResolvesANonEmptyDescriptionKey)
{
	ASSERT_FALSE(qm_card_registry::Defaults().empty());
	for(const qm_card_registry::SCardDefault &Default : qm_card_registry::Defaults())
	{
		SCOPED_TRACE(Default.m_pStableId != nullptr ? Default.m_pStableId : "<null>");
		ASSERT_NE(Default.m_pStableId, nullptr);
		EXPECT_NE(Default.m_pStableId[0], '\0');
		const char *pDescription = qm_card_registry::ResolveDescriptionKey(Default);
		ASSERT_NE(pDescription, nullptr);
		EXPECT_NE(pDescription[0], '\0');
	}
	EXPECT_EQ(qm_card_registry::ResolveLocalizedDescription(static_cast<const char *>(nullptr)), nullptr);
}

TEST(SettingsCardDeck, ScrollMovementOnlySuppressesHoverAfterAnInitializedOffset)
{
	EXPECT_FALSE(SettingsCardDeckScrollMoved(false, 0.0f, 12.0f));
	EXPECT_FALSE(SettingsCardDeckScrollMoved(true, 12.0f, 12.0005f));
	EXPECT_TRUE(SettingsCardDeckScrollMoved(true, 12.0f, 13.0f));
}

TEST(SettingsCardDeck, ActiveCardMotionBlocksHeaderDragStart)
{
	EXPECT_TRUE(SettingsCardDeckAllowsDragStart(false, false, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(true, false, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, true, false, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, false, true, false));
	EXPECT_FALSE(SettingsCardDeckAllowsDragStart(false, false, false, true));
}

TEST(SettingsCardDeck, StableAnimationFramesSkipRuntimeWork)
{
	const SSettingsCardAnimationWork Stable = ResolveSettingsCardAnimationWork(0.16f, false, false, false, 0.18f, false, false);
	EXPECT_FALSE(Stable.m_ResolveEntry);
	EXPECT_FALSE(Stable.m_ResetEntry);
	EXPECT_FALSE(Stable.m_ResolveReflow);
	EXPECT_FALSE(Stable.m_SetReflowTarget);

	const SSettingsCardAnimationWork TargetChanged = ResolveSettingsCardAnimationWork(0.16f, false, false, false, 0.18f, true, false);
	EXPECT_TRUE(TargetChanged.m_ResolveReflow);
	EXPECT_FALSE(TargetChanged.m_SetReflowTarget);

	const SSettingsCardAnimationWork Active = ResolveSettingsCardAnimationWork(0.16f, true, false, false, 0.18f, false, true);
	EXPECT_TRUE(Active.m_ResolveEntry);
	EXPECT_TRUE(Active.m_ResolveReflow);
}

TEST(SettingsCardDeck, DisabledOrSnappedAnimationsOnlyResetTargets)
{
	const SSettingsCardAnimationWork Disabled = ResolveSettingsCardAnimationWork(0.0f, true, false, false, 0.0f, true, true);
	EXPECT_TRUE(Disabled.m_ResetEntry);
	EXPECT_FALSE(Disabled.m_ResolveReflow);
	EXPECT_TRUE(Disabled.m_SetReflowTarget);

	const SSettingsCardAnimationWork Snapped = ResolveSettingsCardAnimationWork(0.16f, false, false, true, 0.18f, true, true);
	EXPECT_FALSE(Snapped.m_ResolveReflow);
	EXPECT_TRUE(Snapped.m_SetReflowTarget);

	const SSettingsCardAnimationWork FirstFrame = ResolveSettingsCardAnimationWork(0.16f, false, true, false, 0.18f, false, false);
	EXPECT_FALSE(FirstFrame.m_SetReflowTarget);
}

TEST(SettingsCardDeck, ContentHeightAnimationSkipsStableFramesAndSnapsFirstLayout)
{
	const SSettingsCardHeightAnimationWork FirstLayout = ResolveSettingsCardHeightAnimationWork(true, false, false, 0.18f, false);
	EXPECT_FALSE(FirstLayout.m_ResolveHeight);
	EXPECT_FALSE(FirstLayout.m_SetHeightTarget);

	const SSettingsCardHeightAnimationWork Stable = ResolveSettingsCardHeightAnimationWork(false, false, false, 0.18f, false);
	EXPECT_FALSE(Stable.m_ResolveHeight);
	EXPECT_FALSE(Stable.m_SetHeightTarget);

	const SSettingsCardHeightAnimationWork Changed = ResolveSettingsCardHeightAnimationWork(false, true, false, 0.18f, false);
	EXPECT_TRUE(Changed.m_ResolveHeight);
	EXPECT_FALSE(Changed.m_SetHeightTarget);

	const SSettingsCardHeightAnimationWork Active = ResolveSettingsCardHeightAnimationWork(false, false, true, 0.18f, false);
	EXPECT_TRUE(Active.m_ResolveHeight);
	EXPECT_FALSE(Active.m_SetHeightTarget);

	const SSettingsCardHeightAnimationWork Disabled = ResolveSettingsCardHeightAnimationWork(false, true, true, 0.0f, false);
	EXPECT_FALSE(Disabled.m_ResolveHeight);
	EXPECT_TRUE(Disabled.m_SetHeightTarget);

	const SSettingsCardHeightAnimationWork DragSnap = ResolveSettingsCardHeightAnimationWork(false, true, true, 0.18f, true);
	EXPECT_FALSE(DragSnap.m_ResolveHeight);
	EXPECT_TRUE(DragSnap.m_SetHeightTarget);
}

TEST(SettingsCardDeck, AnimatedColumnFramesNeverOverlapFollowingCards)
{
	for(const float Progress : {0.0f, 0.2f, 0.5f, 0.8f, 1.0f})
	{
		CSettingsCardColumnFramePlan ColumnPlan(100.0f, 10.0f);
		const SSettingsCardColumnFrame First = ColumnPlan.Append(240.0f + (60.0f - 240.0f) * Progress);
		const SSettingsCardColumnFrame Second = ColumnPlan.Append(90.0f + (180.0f - 90.0f) * Progress);
		const SSettingsCardColumnFrame Third = ColumnPlan.Append(120.0f + (40.0f - 120.0f) * Progress);
		EXPECT_FLOAT_EQ(Second.m_Y, First.m_NextY);
		EXPECT_FLOAT_EQ(Third.m_Y, Second.m_NextY);
		EXPECT_GE(Second.m_Y, First.m_Y + First.m_Height + 10.0f);
		EXPECT_GE(Third.m_Y, Second.m_Y + Second.m_Height + 10.0f);
		EXPECT_FLOAT_EQ(ColumnPlan.CursorY(), Third.m_NextY);
	}

	CSettingsCardColumnFramePlan ClampedPlan(50.0f, -5.0f);
	const SSettingsCardColumnFrame Clamped = ClampedPlan.Append(-20.0f);
	EXPECT_FLOAT_EQ(Clamped.m_Height, 0.0f);
	EXPECT_FLOAT_EQ(Clamped.m_NextY, 50.0f);
}

TEST(SettingsCardDeck, HeightAnimationClipsOnlyTheAnimatedCard)
{
	EXPECT_TRUE(SettingsCardDeckShouldClipContent(true));
	EXPECT_FALSE(SettingsCardDeckShouldClipContent(false));
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
	EXPECT_EQ(aColumns[1], (std::vector<int>{4, 9}));
	EXPECT_EQ(aColumns[2], (std::vector<int>{11, 7}));

	std::array<std::vector<int>, 3> aUnevenColumns{
		std::vector<int>{},
		std::vector<int>{10, 11},
		std::vector<int>{20},
	};
	ApplySettingsCardDeckSingleColumnDragPlacement(aUnevenColumns, 11, 1);
	EXPECT_EQ(aUnevenColumns[1], (std::vector<int>{10, 20}));
	EXPECT_EQ(aUnevenColumns[2], (std::vector<int>{11}));
	std::vector<int> vVisualOrder;
	ForEachSettingsCardDeckVisualOrder(aUnevenColumns, [&](int StateIndex, int Column) {
		if(Column != 0)
			vVisualOrder.push_back(StateIndex);
	});
	EXPECT_EQ(vVisualOrder, (std::vector<int>{10, 11, 20}));
}

TEST(SettingsCardDeck, SingleColumnVisualOrderPreservesWideReadingLayers)
{
	const std::array<std::vector<int>, 3> aColumns{
		std::vector<int>{30},
		std::vector<int>{10, 11},
		std::vector<int>{20},
	};
	std::vector<std::pair<int, int>> vVisualOrder;
	ForEachSettingsCardDeckVisualOrder(aColumns, [&](int StateIndex, int Column) {
		vVisualOrder.emplace_back(StateIndex, Column);
	});
	EXPECT_EQ(vVisualOrder, (std::vector<std::pair<int, int>>{{10, 1}, {20, 2}, {30, 0}, {11, 1}}));
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
	EXPECT_EQ(Model.StableIdOrder("", "sound", 1), (std::vector<std::string>{"left-a", "right-a"}));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"right-b", "left-b"}));
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
	EXPECT_EQ(Model.StableIdOrder("", "sound", 1), (std::vector<std::string>{"left-a", "left-hidden", "right-a"}));
	EXPECT_EQ(Model.StableIdOrder("", "sound", 2), (std::vector<std::string>{"right-hidden", "right-b", "left-b"}));

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
