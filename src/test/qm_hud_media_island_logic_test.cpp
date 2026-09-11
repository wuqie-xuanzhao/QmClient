// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/graphics.h>

#include <game/client/components/hud_media_island_logic.h>
#include <game/client/components/tclient/pet.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace
{
	SHudMediaIslandTrackInput Track(const char *pTitle, const char *pArtist = "", const char *pAlbum = "")
	{
		SHudMediaIslandTrackInput Input;
		Input.m_pTitle = pTitle;
		Input.m_pArtist = pArtist;
		Input.m_pAlbum = pAlbum;
		return Input;
	}

	EHudMediaIslandTrackUpdate ApplyTrack(
		SHudMediaIslandTrackSnapshot &Current,
		SHudMediaIslandTrackSnapshot &Outgoing,
		bool &HasIdentity,
		bool &TransitionActive,
		bool &NeedsNodeReset,
		int64_t &StartTick,
		int64_t Now,
		const SHudMediaIslandTrackInput &Input)
	{
		return QmHudMediaIslandUpdateTrackSnapshots(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, Now, Input);
	}
}

TEST(QmHudMediaIslandLayout, ScalesTheCompleteDesignToEightyPercent)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesignScale, 0.8f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandScaled(16.0f), 12.8f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandScaled(12.0f), 9.6f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandScaled(5.8f), 4.64f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandScaled(3.0f), 2.4f);
}

TEST(QmHudMediaIslandLayout, InfoStackMirrorsRowsAroundTopAnchoredHorizontalMidlineWithCompactGap)
{
	constexpr float IslandY = 0.0f;
	constexpr float IslandHeight = QmHudMediaIslandScaled(16.0f);
	constexpr float TextHeight = QmHudMediaIslandScaled(4.4f);
	constexpr float TextGap = QmHudMediaIslandScaled(0.8f);
	const SHudMediaIslandInfoStackLayout Layout = QmHudMediaIslandMirroredInfoStack(IslandY, IslandHeight, TextHeight, TextGap);
	const float MidY = IslandY + IslandHeight * 0.5f;

	EXPECT_FLOAT_EQ(MidY - Layout.m_TopCenterY, Layout.m_BottomCenterY - MidY);
	EXPECT_FLOAT_EQ(Layout.m_BottomCenterY - Layout.m_TopCenterY, QmHudMediaIslandScaled(5.2f));
	EXPECT_NEAR(
		(Layout.m_BottomCenterY - TextHeight * 0.5f) - (Layout.m_TopCenterY + TextHeight * 0.5f),
		TextGap,
		0.0001f);
}

TEST(QmHudMediaIslandLogic, FirstMediaStateDoesNotStartTrackTransition)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A", "Artist A"));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::FIRST_IDENTITY);
	EXPECT_TRUE(HasIdentity);
	EXPECT_FALSE(TransitionActive);
	EXPECT_FALSE(NeedsNodeReset);
	EXPECT_STREQ(Current.m_aTitle, "Song A");
	EXPECT_FALSE(Outgoing.HasMeaningfulIdentity());
}

TEST(QmHudMediaIslandLogic, FirstOrLateMetadataStartsTheTrackDetailsDeadline)
{
	EXPECT_TRUE(QmHudMediaIslandShouldRevealTrackDetails(EHudMediaIslandTrackUpdate::FIRST_IDENTITY, false, true));
	EXPECT_TRUE(QmHudMediaIslandShouldRevealTrackDetails(EHudMediaIslandTrackUpdate::NONE, false, true));
	EXPECT_FALSE(QmHudMediaIslandShouldRevealTrackDetails(EHudMediaIslandTrackUpdate::NONE, true, true));
	EXPECT_FALSE(QmHudMediaIslandShouldRevealTrackDetails(EHudMediaIslandTrackUpdate::FIRST_IDENTITY, false, false));
	EXPECT_TRUE(QmHudMediaIslandShouldRevealTrackDetails(EHudMediaIslandTrackUpdate::TRACK_CHANGED, true, true));
}

TEST(QmHudMediaIslandLogic, TrackChangeCopiesCurrentSnapshotToOutgoing)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A", "Artist A", "Album A"));
	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 200, Track("Song B", "Artist B", "Album B"));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::TRACK_CHANGED);
	EXPECT_TRUE(TransitionActive);
	EXPECT_TRUE(NeedsNodeReset);
	EXPECT_EQ(StartTick, 200);
	EXPECT_STREQ(Outgoing.m_aTitle, "Song A");
	EXPECT_STREQ(Current.m_aTitle, "Song B");
}

TEST(QmHudMediaIslandLogic, ContinuousTrackChangesKeepOnlyLatestTrack)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A", "Artist A"));
	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 200, Track("Song B", "Artist B"));
	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 250, Track("Song C", "Artist C"));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::TRACK_CHANGED);
	EXPECT_STREQ(Outgoing.m_aTitle, "Song B");
	EXPECT_STREQ(Current.m_aTitle, "Song C");
	EXPECT_TRUE(TransitionActive);
}

TEST(QmHudMediaIslandLogic, TitleChangeWithEmptyArtistStillCountsAsTrackChange)
{
	SHudMediaIslandTrackSnapshot Current;
	Current.SetFrom(Track("Song A"));

	EXPECT_TRUE(QmHudMediaIslandTrackChanged(Current, Track("Song B")));
}

TEST(QmHudMediaIslandLogic, SameTrackRefreshDoesNotRestartTransition)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A", "Artist A"));
	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 200, Track("Song A", "Artist A"));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::NONE);
	EXPECT_FALSE(TransitionActive);
	EXPECT_FALSE(NeedsNodeReset);
	EXPECT_FALSE(Outgoing.HasMeaningfulIdentity());
}

TEST(QmHudMediaIslandLogic, EmptyMetadataRefreshKeepsLastStableSnapshot)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A", "Artist A"));
	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 200, Track(""));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::NONE);
	EXPECT_STREQ(Current.m_aTitle, "Song A");
	EXPECT_STREQ(Current.m_aArtist, "Artist A");
	EXPECT_FALSE(TransitionActive);
}

TEST(QmHudMediaIslandLogic, MissingCoverSnapshotsStillSwitch)
{
	SHudMediaIslandTrackSnapshot Current;
	SHudMediaIslandTrackSnapshot Outgoing;
	bool HasIdentity = false;
	bool TransitionActive = false;
	bool NeedsNodeReset = false;
	int64_t StartTick = 0;

	ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 100, Track("Song A"));
	const EHudMediaIslandTrackUpdate Update = ApplyTrack(Current, Outgoing, HasIdentity, TransitionActive, NeedsNodeReset, StartTick, 200, Track("Song B"));

	EXPECT_EQ(Update, EHudMediaIslandTrackUpdate::TRACK_CHANGED);
	EXPECT_FALSE(Current.m_HasCover);
	EXPECT_FALSE(Outgoing.m_HasCover);
}

TEST(QmHudMediaIslandLogic, Utf8TitlesStayNulTerminatedInFixedSnapshot)
{
	SHudMediaIslandTrackSnapshot Current;
	const char *pTitle = "很长的中文标题 Mixed UTF-8 Title 很长的中文标题 Mixed UTF-8 Title 很长的中文标题 Mixed UTF-8 Title 很长的中文标题 Mixed UTF-8 Title";

	Current.SetFrom(Track(pTitle, "艺术家"));

	EXPECT_NE(Current.m_aTitle[0], '\0');
	EXPECT_EQ(Current.m_aTitle[sizeof(Current.m_aTitle) - 1], '\0');
	EXPECT_STREQ(Current.m_aArtist, "艺术家");
}

TEST(QmHudMediaIslandLogic, TopEffectMovesBelowOverlappingIsland)
{
	const CUIRect Island = {120.0f, 0.0f, 80.0f, 38.0f};

	EXPECT_FLOAT_EQ(QmHudTopEffectY(20.0f, 10.0f, 140.0f, 180.0f, Island, true), 41.0f);
}

TEST(QmHudMediaIslandLogic, TopEffectDoesNotMoveForHorizontalSeparationOrHiddenIsland)
{
	const CUIRect SideIsland = {20.0f, 0.0f, 60.0f, 38.0f};
	const CUIRect CenterIsland = {120.0f, 0.0f, 80.0f, 38.0f};

	EXPECT_FLOAT_EQ(QmHudTopEffectY(20.0f, 10.0f, 140.0f, 180.0f, SideIsland, true), 20.0f);
	EXPECT_FLOAT_EQ(QmHudTopEffectY(20.0f, 10.0f, 140.0f, 180.0f, CenterIsland, false), 20.0f);
}

TEST(QmHudMediaIslandLogic, TopEffectDoesNotMoveForIslandBelowIt)
{
	const CUIRect LowerIsland = {120.0f, 100.0f, 80.0f, 38.0f};

	EXPECT_FLOAT_EQ(QmHudTopEffectY(20.0f, 24.0f, 140.0f, 180.0f, LowerIsland, true), 20.0f);
}

TEST(QmHudMediaIslandLogic, TeamZeroDoesNotCreateTeamDisplay)
{
	EXPECT_FALSE(QmHudMediaIslandShouldShowTeam(true, true, 0));
	EXPECT_TRUE(QmHudMediaIslandShouldShowTeam(true, true, 1));
	EXPECT_FALSE(QmHudMediaIslandShouldShowTeam(false, true, 1));
	EXPECT_FALSE(QmHudMediaIslandShouldShowTeam(true, false, 1));
}

TEST(QmHudMediaIslandEntrance, StartsAsOpaqueBlackCircleAtTargetCenter)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);

	const SHudMediaIslandEntrancePose Pose = QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.0f);

	EXPECT_FLOAT_EQ(Pose.m_Rect.x, 133.6f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.y, 10.6f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.w, 12.8f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.h, 12.8f);
	EXPECT_FLOAT_EQ(Pose.m_Radius, 6.4f);
	EXPECT_FLOAT_EQ(Pose.m_DisabledCornerRadius, 6.4f);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.r, 0.0f);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.g, 0.0f);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.b, 0.0f);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.a, 1.0f);
	EXPECT_FLOAT_EQ(Pose.m_ContentAlpha, 0.0f);
}

TEST(QmHudMediaIslandEntrance, SettlesExactlyAtConfiguredAppearance)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);

	const SHudMediaIslandEntrancePose Pose = QmHudMediaIslandEntrancePose(Target, 6.0f, TargetColor, 1.0f);

	EXPECT_FLOAT_EQ(Pose.m_Rect.x, Target.x);
	EXPECT_FLOAT_EQ(Pose.m_Rect.y, Target.y);
	EXPECT_FLOAT_EQ(Pose.m_Rect.w, Target.w);
	EXPECT_FLOAT_EQ(Pose.m_Rect.h, Target.h);
	EXPECT_FLOAT_EQ(Pose.m_Radius, 6.0f);
	EXPECT_FLOAT_EQ(Pose.m_DisabledCornerRadius, 0.0f);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.r, TargetColor.r);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.g, TargetColor.g);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.b, TargetColor.b);
	EXPECT_FLOAT_EQ(Pose.m_BackgroundColor.a, TargetColor.a);
	EXPECT_FLOAT_EQ(Pose.m_ContentAlpha, 1.0f);
}

TEST(QmHudMediaIslandEntrance, ProgressesForwardAndMotionDisabledSnapsToSettled)
{
	EXPECT_GT(QmHudAdvanceMediaIslandEntranceProgress(0.0f, 0.10f, 2), 0.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandEntranceProgress(0.4f, -1.0f, 2), 0.4f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandEntranceProgress(0.95f, 1.0f, 2), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandEntranceProgress(0.4f, 0.0f, 0), 1.0f);
}

TEST(QmHudMediaIslandEntrance, ReducedMotionUsesProjectShortenedDuration)
{
	const float FullMotionProgress = QmHudAdvanceMediaIslandEntranceProgress(0.0f, 0.10f, 2);
	const float ReducedMotionProgress = QmHudAdvanceMediaIslandEntranceProgress(0.0f, 0.10f, 1);

	EXPECT_GT(ReducedMotionProgress, FullMotionProgress);
	EXPECT_LT(ReducedMotionProgress, 1.0f);
}

TEST(QmHudMediaIslandEntrance, DropStartsFullyAboveScreenAndEndsAtExpansionOrigin)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);
	constexpr float ScreenTop = -20.0f;

	const SHudMediaIslandEntrancePose Hidden = QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.0f, 0.0f, ScreenTop);
	const SHudMediaIslandEntrancePose Arrived = QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.0f, 1.0f, ScreenTop);

	EXPECT_LT(Hidden.m_Rect.y + Hidden.m_Rect.h, ScreenTop);
	EXPECT_FLOAT_EQ(Hidden.m_Rect.x + Hidden.m_Rect.w * 0.5f, Target.x + Target.w * 0.5f);
	EXPECT_FLOAT_EQ(Arrived.m_Rect.y, Target.y + Target.h * 0.5f - 6.4f);
	EXPECT_FLOAT_EQ(Arrived.m_Rect.w, 12.8f);
	EXPECT_FLOAT_EQ(Arrived.m_Rect.h, 12.8f);
	EXPECT_FLOAT_EQ(Arrived.m_ContentAlpha, 0.0f);
}

TEST(QmHudMediaIslandEntrance, DropUsesDedicatedDurationAndReducedMotionRule)
{
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandEntranceDropProgress(0.0f, 0.18f, 2), 1.0f);
	EXPECT_GT(QmHudAdvanceMediaIslandEntranceDropProgress(0.0f, 0.04f, 1), QmHudAdvanceMediaIslandEntranceDropProgress(0.0f, 0.04f, 2));
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandEntranceDropProgress(0.4f, 0.0f, 0), 1.0f);
}

TEST(QmHudMediaIslandEntrance, TimelineWaitsOnePhaseBoundaryBeforeExpanding)
{
	SHudMediaIslandEntranceTimeline Timeline;
	Timeline = QmHudAdvanceMediaIslandEntranceTimeline(Timeline, 0.18f, 2);
	EXPECT_FLOAT_EQ(Timeline.m_DropProgress, 1.0f);
	EXPECT_FLOAT_EQ(Timeline.m_ExpandProgress, 0.0f);

	Timeline = QmHudAdvanceMediaIslandEntranceTimeline(Timeline, 0.01f, 2);
	EXPECT_GT(Timeline.m_ExpandProgress, 0.0f);

	const SHudMediaIslandEntranceTimeline MotionDisabled = QmHudAdvanceMediaIslandEntranceTimeline({}, 0.0f, 0);
	EXPECT_FLOAT_EQ(MotionDisabled.m_DropProgress, 1.0f);
	EXPECT_FLOAT_EQ(MotionDisabled.m_ExpandProgress, 1.0f);
}

TEST(QmHudMediaIslandEntrance, KeepsContentHiddenUntilShapeNearlySettlesThenFadesItIn)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);

	EXPECT_FLOAT_EQ(QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.92f).m_ContentAlpha, 0.0f);
	EXPECT_GT(QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.96f).m_ContentAlpha, 0.0f);
	EXPECT_LT(QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.96f).m_ContentAlpha, 1.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 1.0f).m_ContentAlpha, 1.0f);

	const CUIRect WideTarget = {0.0f, 1.0f, 300.0f, 32.0f};
	const SHudMediaIslandEntrancePose FirstVisibleContent = QmHudMediaIslandEntrancePose(WideTarget, 8.0f, TargetColor, 0.93f);
	EXPECT_GT(FirstVisibleContent.m_ContentAlpha, 0.0f);
	EXPECT_LE(FirstVisibleContent.m_Rect.x, WideTarget.x + 2.05f);
	EXPECT_GE(FirstVisibleContent.m_Rect.x + FirstVisibleContent.m_Rect.w, WideTarget.x + WideTarget.w - 2.05f);
}

TEST(QmHudMediaIslandEntrance, IntermediatePoseMorphsGeometryAndConfiguredBackgroundTogether)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);

	const SHudMediaIslandEntrancePose Pose = QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.5f);

	EXPECT_GT(Pose.m_Rect.w, 12.8f);
	EXPECT_LT(Pose.m_Rect.w, Target.w);
	EXPECT_GT(Pose.m_Rect.h, 12.8f);
	EXPECT_LT(Pose.m_Rect.h, Target.h);
	EXPECT_GT(Pose.m_BackgroundColor.b, 0.0f);
	EXPECT_LT(Pose.m_BackgroundColor.b, TargetColor.b);
	EXPECT_GT(Pose.m_BackgroundColor.a, TargetColor.a);
	EXPECT_LT(Pose.m_BackgroundColor.a, 1.0f);
}

TEST(QmHudMediaIslandSatellite, SortsByTypeThenTriggerOrder)
{
	std::array<SHudMediaIslandCountdownInput, 6> aInputs = {{
		{EHudMediaIslandCountdownType::MUTE, 0, 10, 70, 60},
		{EHudMediaIslandCountdownType::SWITCH, 4, 30, 80, 50},
		{EHudMediaIslandCountdownType::SWAP, 1, 20, 50, 30},
		{EHudMediaIslandCountdownType::SWITCH, 2, 12, 62, 50},
		{EHudMediaIslandCountdownType::SWAP, 0, 5, 35, 30},
		{EHudMediaIslandCountdownType::SWITCH, 9, 30, 90, 60},
	}};

	QmHudSortMediaIslandCountdowns(aInputs.data(), aInputs.size());

	EXPECT_EQ(aInputs[0].m_Type, EHudMediaIslandCountdownType::SWAP);
	EXPECT_EQ(aInputs[0].m_Id, 0);
	EXPECT_EQ(aInputs[1].m_Type, EHudMediaIslandCountdownType::SWAP);
	EXPECT_EQ(aInputs[1].m_Id, 1);
	EXPECT_EQ(aInputs[2].m_Type, EHudMediaIslandCountdownType::SWITCH);
	EXPECT_EQ(aInputs[2].m_Id, 2);
	EXPECT_EQ(aInputs[3].m_Type, EHudMediaIslandCountdownType::SWITCH);
	EXPECT_EQ(aInputs[3].m_Id, 4);
	EXPECT_EQ(aInputs[4].m_Type, EHudMediaIslandCountdownType::SWITCH);
	EXPECT_EQ(aInputs[4].m_Id, 9);
	EXPECT_EQ(aInputs[5].m_Type, EHudMediaIslandCountdownType::MUTE);
}

TEST(QmHudMediaIslandSatellite, ProgressClampsAtLifecycleBounds)
{
	const SHudMediaIslandCountdownInput Input{EHudMediaIslandCountdownType::SWAP, 0, 100, 400, 300};

	EXPECT_FLOAT_EQ(QmHudMediaIslandCountdownProgress(Input, 50), 1.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandCountdownProgress(Input, 250), 0.5f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandCountdownProgress(Input, 450), 0.0f);
}

TEST(QmHudMediaIslandSatellite, MultipleItemsKeepThreePixelEdgeGap)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandSatelliteWidth(0, 16.0f, 3.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSatelliteWidth(1, 16.0f, 3.0f), 16.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSatelliteWidth(2, 16.0f, 3.0f), 35.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSatelliteWidth(3, 16.0f, 3.0f), 54.0f);
}

TEST(QmHudMediaIslandLayout, ActiveLyricsKeepAFixedViewportWithAnExistingTopRow)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, false, 0.0f, 72.0f, 300.0f, 10.0f), 92.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, false, 0.0f, 500.0f, 300.0f, 10.0f), 300.0f);
}

TEST(QmHudMediaIslandLayout, LyricsOnlyUsesFixedTitleAreaWidth)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, false, false, 0.0f, 72.0f, 300.0f, 10.0f), 92.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, false, false, 0.0f, 500.0f, 100.0f, 10.0f), 100.0f);
}

TEST(QmHudMediaIslandLayout, UtilityBottomContentStillControlsRequestedWidth)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, true, 45.0f, 72.0f, 300.0f, 10.0f), 92.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(false, false, true, 45.0f, 72.0f, 300.0f, 10.0f), 65.0f);
}

TEST(QmHudMediaIslandLyrics, ActiveLyricsKeepTheIslandExpandedWithoutRepeatedMorphs)
{
	SHudMediaIslandExpansionState State;
	State = QmHudMediaIslandUpdateExpansion(State, true, true, false, 1000, 3000);
	EXPECT_TRUE(State.m_Expanded);
	EXPECT_TRUE(State.m_LyricsActive);
	EXPECT_EQ(State.m_ExpandUntilTick, 0);
	EXPECT_TRUE(State.m_StartCapsuleMorph);

	State.m_StartCapsuleMorph = false;
	State = QmHudMediaIslandUpdateExpansion(State, true, true, false, 100000, 3000);
	EXPECT_TRUE(State.m_Expanded);
	EXPECT_EQ(State.m_ExpandUntilTick, 0);
	EXPECT_FALSE(State.m_StartCapsuleMorph);
}

TEST(QmHudMediaIslandLyrics, TrackDetailsUseAnIndependentThreeSecondDeadline)
{
	EXPECT_TRUE(QmHudMediaIslandShouldShowTrackDetails(1000, 4000));
	EXPECT_FALSE(QmHudMediaIslandShouldShowTrackDetails(4000, 4000));
	EXPECT_FALSE(QmHudMediaIslandShouldShowTrackDetails(5000, 0));
}

TEST(QmHudMediaIslandLyrics, LosingLyricsRestoresTheNormalAutoCollapseDeadline)
{
	SHudMediaIslandExpansionState State;
	State.m_Expanded = true;
	State.m_LyricsActive = true;
	State = QmHudMediaIslandUpdateExpansion(State, true, false, false, 1000, 3000);
	EXPECT_TRUE(State.m_Expanded);
	EXPECT_EQ(State.m_ExpandUntilTick, 4000);
	EXPECT_FALSE(State.m_StartCapsuleMorph);

	State = QmHudMediaIslandUpdateExpansion(State, true, false, false, 4000, 3000);
	EXPECT_FALSE(State.m_Expanded);
	EXPECT_EQ(State.m_ExpandUntilTick, 0);
	EXPECT_TRUE(State.m_StartCapsuleMorph);
}

TEST(QmHudMediaIslandLyrics, ShortLyricsNeverScroll)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandMarqueeOffset(80.0f, 100.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandMarqueeOffset(100.0f, 100.0f, 500.0f), 0.0f);
}

TEST(QmHudMediaIslandLyrics, LongLyricsPauseTravelAndReturnWithinTheViewport)
{
	constexpr float TextWidth = 200.0f;
	constexpr float ViewportWidth = 100.0f;
	constexpr float Speed = 50.0f;
	EXPECT_FLOAT_EQ(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 0.0f, Speed), 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 1.2f, Speed), 0.0f);
	EXPECT_NEAR(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 2.2f, Speed), 50.0f, 0.001f);
	EXPECT_NEAR(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 3.2f, Speed), 100.0f, 0.001f);
	EXPECT_NEAR(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 4.2f, Speed), 100.0f, 0.001f);
	EXPECT_NEAR(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 5.2f, Speed), 50.0f, 0.001f);
	EXPECT_NEAR(QmHudMediaIslandMarqueeOffset(TextWidth, ViewportWidth, 6.2f, Speed), 0.0f, 0.001f);
}

TEST(QmHudMediaIslandLyrics, NewLineResetsTheMarqueeAtItsStartingPause)
{
	EXPECT_FALSE(QmHudMediaIslandShouldResetMarquee("same", "same"));
	EXPECT_TRUE(QmHudMediaIslandShouldResetMarquee("first", "second"));
	EXPECT_TRUE(QmHudMediaIslandShouldResetMarquee(nullptr, "line"));
	EXPECT_FLOAT_EQ(QmHudMediaIslandMarqueeOffset(200.0f, 100.0f, 0.0f), 0.0f);
}

TEST(QmHudMediaIslandLayout, FirstIncomingSwapReplacesCheckpointAndLyricsRemainLast)
{
	const SHudMediaIslandSwapRows Rows = QmHudMediaIslandSwapRows(3, true, true);
	EXPECT_EQ(Rows.m_InlineSwapCount, 1);
	EXPECT_EQ(Rows.m_BottomSwapCount, 2);
	EXPECT_EQ(Rows.m_BottomLineCount, 3);
	EXPECT_EQ(Rows.m_LyricsLineIndex, 2);
}

TEST(QmHudMediaIslandLayout, SwapsUseBottomRowsWhenRaceTimerIsUnavailable)
{
	const SHudMediaIslandSwapRows Rows = QmHudMediaIslandSwapRows(3, false, true);
	EXPECT_EQ(Rows.m_InlineSwapCount, 0);
	EXPECT_EQ(Rows.m_BottomSwapCount, 3);
	EXPECT_EQ(Rows.m_BottomLineCount, 4);
	EXPECT_EQ(Rows.m_LyricsLineIndex, 3);
}

TEST(QmHudMediaIslandSatellite, KeepsLatestVisibleSwitchesAndSeparatesTeamIdentity)
{
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(5, 3), 2);
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(3, 3), 0);
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(2, 0), 2);
	EXPECT_NE(QmHudMediaIslandSwitchInstanceId(1, 7), QmHudMediaIslandSwitchInstanceId(2, 7));
	EXPECT_EQ(QmHudMediaIslandSwitchInstanceId(2, 7) & 0xff, 7);
}

TEST(QmHudSwitchCountdown, SelectsTheLatestThreeActiveTriggersAndKeepsTheirOwners)
{
	const std::array<SHudSwitchCountdownEntry, 5> aEntries = {{
		{1, 4, 11, 0, 10, 90, 50},
		{1, 7, 12, 1, 40, 100, 55},
		{2, 3, 11, 0, 30, 110, 50},
		{2, 9, 12, 1, 60, 45, 50},
		{1, 8, 11, 0, 50, 120, 50},
	}};
	std::array<SHudSwitchCountdownEntry, 3> aSelected{};

	const int Count = QmHudSelectLatestSwitchCountdowns(aEntries.data(), aEntries.size(), aSelected.data(), aSelected.size());

	ASSERT_EQ(Count, 3);
	EXPECT_EQ(aSelected[0].m_Number, 8);
	EXPECT_EQ(aSelected[0].m_ClientId, 11);
	EXPECT_EQ(aSelected[1].m_Number, 7);
	EXPECT_EQ(aSelected[1].m_ClientId, 12);
	EXPECT_EQ(aSelected[2].m_Number, 3);
	EXPECT_EQ(aSelected[2].m_ClientId, 11);
}

TEST(QmHudSwitchCountdown, FollowTargetsDefaultLeftAndStayOppositeThePet)
{
	const vec2 TeePosition(100.0f, 200.0f);
	EXPECT_EQ(QmHudSwitchCountdownFollowSide(TeePosition.x, false, 0.0f), -1);
	EXPECT_EQ(QmHudSwitchCountdownFollowSide(TeePosition.x, true, 60.0f), 1);
	EXPECT_EQ(QmHudSwitchCountdownFollowSide(TeePosition.x, true, 140.0f), -1);

	const vec2 NearestLeft = QmHudSwitchCountdownFollowTarget(TeePosition, -1, 0, 0.0f);
	const vec2 OlderLeft = QmHudSwitchCountdownFollowTarget(TeePosition, -1, 1, 0.0f);
	const vec2 NearestRight = QmHudSwitchCountdownFollowTarget(TeePosition, 1, 0, 0.0f);
	const vec2 OlderRight = QmHudSwitchCountdownFollowTarget(TeePosition, 1, 1, 0.0f);
	EXPECT_GT(NearestLeft.x, OlderLeft.x);
	EXPECT_LT(NearestRight.x, OlderRight.x);
	EXPECT_LT(NearestLeft.y, TeePosition.y);
	EXPECT_FLOAT_EQ(NearestLeft.y, OlderLeft.y);
}

TEST(QmHudSwitchCountdown, LocationModeKeepsLegacyValuesAndAllowsBothSurfaces)
{
	const int FollowTee = static_cast<int>(EQmSwitchCountdownMode::FOLLOW_TEE);
	const int MediaIsland = static_cast<int>(EQmSwitchCountdownMode::MEDIA_ISLAND);
	const int Both = static_cast<int>(EQmSwitchCountdownMode::BOTH);

	EXPECT_TRUE(QmHudSwitchCountdownShowsFollowTee(FollowTee));
	EXPECT_FALSE(QmHudSwitchCountdownShowsMediaIsland(FollowTee));
	EXPECT_FALSE(QmHudSwitchCountdownShowsFollowTee(MediaIsland));
	EXPECT_TRUE(QmHudSwitchCountdownShowsMediaIsland(MediaIsland));
	EXPECT_TRUE(QmHudSwitchCountdownShowsFollowTee(Both));
	EXPECT_TRUE(QmHudSwitchCountdownShowsMediaIsland(Both));

	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(true, false, MediaIsland), FollowTee);
	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(false, true, FollowTee), MediaIsland);
	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(true, true, FollowTee), Both);
	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(false, false, FollowTee), FollowTee);
	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(false, false, MediaIsland), MediaIsland);
	EXPECT_EQ(QmHudSwitchCountdownModeFromLocations(false, false, Both), Both);
}

TEST(QmHudMediaIslandBlob, CriticallyDampedTravelIsContinuousAndSettlesWithinTheTimeline)
{
	const SHudMediaIslandBlobPose Hidden = QmHudMediaIslandBlobPose(0.0f);
	const SHudMediaIslandBlobPose Quarter = QmHudMediaIslandBlobPose(0.25f);
	const SHudMediaIslandBlobPose Half = QmHudMediaIslandBlobPose(0.50f);
	const SHudMediaIslandBlobPose Late = QmHudMediaIslandBlobPose(0.75f);
	const SHudMediaIslandBlobPose Settled = QmHudMediaIslandBlobPose(1.0f);

	EXPECT_FLOAT_EQ(Hidden.m_Travel, 0.0f);
	EXPECT_GT(Quarter.m_Travel, Hidden.m_Travel);
	EXPECT_GT(Half.m_Travel, Quarter.m_Travel);
	EXPECT_GT(Late.m_Travel, Half.m_Travel);
	EXPECT_LT(Late.m_Travel, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_Travel, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_RadiusScale, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_StretchX, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_StretchY, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_ContentAlpha, 1.0f);
}

TEST(QmHudMediaIslandBlob, VelocityStretchIsSubtleAndReturnsToACircleAtRest)
{
	const SHudMediaIslandBlobPose Moving = QmHudMediaIslandBlobPose(0.20f);
	const SHudMediaIslandBlobPose Settled = QmHudMediaIslandBlobPose(1.0f);
	EXPECT_GT(Moving.m_StretchX, 1.0f);
	EXPECT_LT(Moving.m_StretchY, 1.0f);
	EXPECT_LE(Moving.m_StretchX, 1.065f);
	EXPECT_GE(Moving.m_StretchY, 0.965f);
	EXPECT_FLOAT_EQ(Settled.m_StretchX, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_StretchY, 1.0f);
}

TEST(QmHudMediaIslandBlob, ProgressCanReverseWithoutPoseDiscontinuity)
{
	float Progress = QmHudAdvanceMediaIslandLiquidProgress(0.0f, true, 0.220f, true);
	const SHudMediaIslandBlobPose BeforeReverse = QmHudMediaIslandBlobPose(Progress);
	Progress = QmHudAdvanceMediaIslandLiquidProgress(Progress, false, 0.110f, true);
	Progress = QmHudAdvanceMediaIslandLiquidProgress(Progress, true, 0.110f, true);
	const SHudMediaIslandBlobPose AfterReverse = QmHudMediaIslandBlobPose(Progress);
	EXPECT_NEAR(AfterReverse.m_Travel, BeforeReverse.m_Travel, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_RadiusScale, BeforeReverse.m_RadiusScale, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_StretchX, BeforeReverse.m_StretchX, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_StretchY, BeforeReverse.m_StretchY, 0.0001f);
}

TEST(QmHudMediaIslandBlob, SmoothMergeDetachesAtRestAndRemainsDuringTravel)
{
	const float Blend = QmHudMediaIslandBlobBlend(8.0f, 1.0f);
	EXPECT_GT(Blend, 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandBlobBlend(8.0f, 0.0f), 0.0f);
	const float MovingConnection = QmHudMediaIslandBlobConnectionStrength(QmHudMediaIslandBlobPose(0.20f).m_Travel);
	const float SettledConnection = QmHudMediaIslandBlobConnectionStrength(QmHudMediaIslandBlobPose(1.0f).m_Travel);
	EXPECT_GT(MovingConnection, 0.0f);
	EXPECT_FLOAT_EQ(SettledConnection, 0.0f);
	const float NearBridge = QmHudMediaIslandSdfSmoothUnion(1.0f, 1.0f, Blend * MovingConnection);
	const float SettledGap = QmHudMediaIslandSdfSmoothUnion(1.5f, 1.5f, Blend * SettledConnection);
	EXPECT_LT(NearBridge, 0.0f);
	EXPECT_FLOAT_EQ(SettledGap, 1.5f);
}

TEST(QmHudMediaIslandSatellite, LiquidProgressClampsAndReducedMotionSnaps)
{
	EXPECT_LT(QmHudAdvanceMediaIslandLiquidProgress(0.0f, true, 0.439f, true), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.0f, true, 0.440f, true), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.95f, true, 1.0f, true), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.05f, false, 1.0f, true), 0.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, true, -1.0f, true), 0.4f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, true, 0.01f, false), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, false, 0.01f, false), 0.0f);
}

TEST(QmHudMediaIslandSpectatorEye, OpeningTransitionHonorsMotionLevel)
{
	EXPECT_LT(QmHudAdvanceMediaIslandSpectatorIconProgress(0.0f, 0.179f, 2), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandSpectatorIconProgress(0.0f, 0.180f, 2), 1.0f);
	EXPECT_LT(QmHudAdvanceMediaIslandSpectatorIconProgress(0.0f, 0.080f, 1), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandSpectatorIconProgress(0.0f, 0.081f, 1), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandSpectatorIconProgress(0.3f, 0.001f, 0), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandSpectatorIconProgress(0.3f, -1.0f, 2), 0.3f);
}

TEST(QmHudMediaIslandSpectatorEye, ApprovedOpeningPoseCrossfadesAndOpensVertically)
{
	const SHudMediaIslandSpectatorIconPose Closed = QmHudMediaIslandSpectatorIconPose(0.0f);
	EXPECT_FLOAT_EQ(Closed.m_ClosedAlpha, 1.0f);
	EXPECT_FLOAT_EQ(Closed.m_OpenAlpha, 0.0f);
	EXPECT_FLOAT_EQ(Closed.m_OpenScaleX, 0.88f);
	EXPECT_FLOAT_EQ(Closed.m_OpenScaleY, 0.44f);
	EXPECT_FLOAT_EQ(Closed.m_CountAlpha, 0.0f);
	EXPECT_FLOAT_EQ(Closed.m_CountOffsetX, -2.4f);

	const SHudMediaIslandSpectatorIconPose Mid = QmHudMediaIslandSpectatorIconPose(0.5f);
	EXPECT_FLOAT_EQ(Mid.m_ClosedAlpha, 0.5f);
	EXPECT_FLOAT_EQ(Mid.m_OpenAlpha, 0.5f);

	const SHudMediaIslandSpectatorIconPose Open = QmHudMediaIslandSpectatorIconPose(1.0f);
	EXPECT_FLOAT_EQ(Open.m_ClosedAlpha, 0.0f);
	EXPECT_FLOAT_EQ(Open.m_OpenAlpha, 1.0f);
	EXPECT_FLOAT_EQ(Open.m_OpenScaleX, 1.0f);
	EXPECT_FLOAT_EQ(Open.m_OpenScaleY, 1.0f);
	EXPECT_FLOAT_EQ(Open.m_CountAlpha, 1.0f);
	EXPECT_FLOAT_EQ(Open.m_CountOffsetX, 0.0f);
}

TEST(QmHudMediaIslandSpectatorEye, ClosingProgressFollowsTheRightCapsuleRetraction)
{
	const float LiquidProgress = QmHudAdvanceMediaIslandLiquidProgress(1.0f, false, 0.220f, true);
	const float IconProgress = QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 1.0f, LiquidProgress);
	EXPECT_NEAR(IconProgress, 0.5f, 0.0001f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 0.25f, 0.25f), 1.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 0.25f, 0.125f), 0.5f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 0.25f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 0.0f, 0.0f), 0.0f);

	const SHudMediaIslandSpectatorIconPose HalfClosed = QmHudMediaIslandSpectatorIconPose(IconProgress);
	EXPECT_FLOAT_EQ(HalfClosed.m_OpenAlpha, 0.5f);
	EXPECT_FLOAT_EQ(HalfClosed.m_ClosedAlpha, 0.5f);
}

TEST(QmHudMediaIslandSpectatorEye, ReopeningContinuesFromTheCurrentClosingPose)
{
	const float HalfClosed = QmHudMediaIslandSpectatorIconProgressDuringExit(1.0f, 1.0f, 0.5f);
	const float Reopened = QmHudAdvanceMediaIslandSpectatorIconProgress(HalfClosed, 0.045f, 2);
	EXPECT_NEAR(Reopened, 0.75f, 0.0001f);
}

TEST(QmHudMediaIslandSpectatorEye, ReopensOnlyWhileTheRightCapsuleIsBeingReclaimed)
{
	EXPECT_TRUE(QmHudMediaIslandShouldAnimateSpectatorEyeOpen(true, false, 0.4f));
	EXPECT_FALSE(QmHudMediaIslandShouldAnimateSpectatorEyeOpen(true, false, 0.0f));
	EXPECT_FALSE(QmHudMediaIslandShouldAnimateSpectatorEyeOpen(true, true, 0.4f));
	EXPECT_FALSE(QmHudMediaIslandShouldAnimateSpectatorEyeOpen(false, false, 0.4f));
	EXPECT_FLOAT_EQ(QmHudMediaIslandSpectatorCountAlpha(false, QmHudMediaIslandSpectatorIconPose(1.0f)), 0.0f);
}

TEST(QmHudMediaIslandBlob, RightCapsuleSettlesOutsideMainIsland)
{
	const SHudMediaIslandLiquidCapsule Capsule = QmHudMediaIslandRightBlobCapsule(100.0f, 20.0f, 8.0f, 24.0f, 4.0f, QmHudMediaIslandBlobPose(1.0f));

	EXPECT_FLOAT_EQ(Capsule.m_Rect.x, 104.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.y, 12.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.w, 24.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.h, 16.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Radius, 8.0f);
	EXPECT_FLOAT_EQ(Capsule.m_SmoothUnion, 0.0f);
	EXPECT_FLOAT_EQ(Capsule.m_ContentAlpha, 1.0f);
}

TEST(QmHudMediaIslandBlob, RightCapsuleUsesTheSameBoundedVelocityStretch)
{
	const SHudMediaIslandBlobPose MovingPose = QmHudMediaIslandBlobPose(0.20f);
	const SHudMediaIslandLiquidCapsule Capsule = QmHudMediaIslandRightBlobCapsule(100.0f, 20.0f, 8.0f, 24.0f, 4.0f, MovingPose);
	EXPECT_GT(Capsule.m_Rect.w, Capsule.m_Rect.h);
	EXPECT_GT(Capsule.m_SmoothUnion, 0.0f);
	EXPECT_GT(Capsule.m_ContentAlpha, 0.0f);
}

TEST(QmHudMediaIslandSatellite, SwapCompletionKeepsIdentityAndDoesNotRestartProgressRing)
{
	constexpr int64_t StartTick = 100;
	constexpr int TickSpeed = 50;
	const SHudMediaIslandSwapLifecycle Countdown = QmHudMediaIslandSwapLifecycle(StartTick, StartTick + 30 * TickSpeed - 1, TickSpeed);
	const SHudMediaIslandSwapLifecycle Ready = QmHudMediaIslandSwapLifecycle(StartTick, StartTick + 30 * TickSpeed, TickSpeed);
	const SHudMediaIslandCountdownInput CountdownSwap = QmHudMediaIslandSwapCountdownInput(1, StartTick, Countdown, false);
	const SHudMediaIslandCountdownInput ReadySwap = QmHudMediaIslandSwapCountdownInput(1, StartTick, Ready, false);

	EXPECT_TRUE(Countdown.m_Visible);
	EXPECT_FALSE(Countdown.m_Completed);
	EXPECT_EQ(Countdown.m_SecondsLeft, 1);
	EXPECT_GT(CountdownSwap.m_Progress, 0.0f);
	EXPECT_TRUE(Ready.m_Visible);
	EXPECT_TRUE(Ready.m_Completed);
	EXPECT_FLOAT_EQ(ReadySwap.m_Progress, 0.0f);
	EXPECT_EQ(ReadySwap.m_Type, CountdownSwap.m_Type);
	EXPECT_EQ(ReadySwap.m_Id, CountdownSwap.m_Id);
	EXPECT_TRUE(ReadySwap.m_Completed);
}

TEST(QmHudMediaIslandSatellite, SwapReadyStateExpiresAtSixtySeconds)
{
	constexpr int64_t StartTick = 100;
	constexpr int TickSpeed = 50;
	const SHudMediaIslandSwapLifecycle LastReadyTick = QmHudMediaIslandSwapLifecycle(StartTick, StartTick + 60 * TickSpeed - 1, TickSpeed);
	const SHudMediaIslandSwapLifecycle Expired = QmHudMediaIslandSwapLifecycle(StartTick, StartTick + 60 * TickSpeed, TickSpeed);

	EXPECT_TRUE(LastReadyTick.m_Visible);
	EXPECT_TRUE(LastReadyTick.m_Completed);
	EXPECT_FALSE(Expired.m_Visible);
}

TEST(QmHudMediaIslandSatellite, SwapDirectionAndConnectionStayBoundToTheirInstance)
{
	const SHudMediaIslandSwapLifecycle Lifecycle = QmHudMediaIslandSwapLifecycle(100, 200, 50);
	const SHudMediaIslandCountdownInput Incoming = QmHudMediaIslandSwapCountdownInput(0, 100, Lifecycle, false);
	const SHudMediaIslandCountdownInput Outgoing = QmHudMediaIslandSwapCountdownInput(1, 100, Lifecycle, true);

	EXPECT_FALSE(Incoming.m_SwapOutgoing);
	EXPECT_TRUE(Outgoing.m_SwapOutgoing);
	EXPECT_TRUE(QmHudMediaIslandSwapVisibleForConnection(Incoming.m_Id, 0));
	EXPECT_FALSE(QmHudMediaIslandSwapVisibleForConnection(Incoming.m_Id, 1));
	EXPECT_TRUE(QmHudMediaIslandSwapVisibleForConnection(Outgoing.m_Id, 1));
	EXPECT_FALSE(QmHudMediaIslandSwapVisibleForConnection(Outgoing.m_Id, 0));
}

TEST(QmHudMediaIslandSatellite, SdfCircleUsesNegativeInsideAndPositiveOutside)
{
	EXPECT_LT(QmHudMediaIslandSdfCircle(vec2(0.0f, 0.0f), vec2(0.0f, 0.0f), 2.0f), 0.0f);
	EXPECT_NEAR(QmHudMediaIslandSdfCircle(vec2(2.0f, 0.0f), vec2(0.0f, 0.0f), 2.0f), 0.0f, 0.0001f);
	EXPECT_GT(QmHudMediaIslandSdfCircle(vec2(3.0f, 0.0f), vec2(0.0f, 0.0f), 2.0f), 0.0f);
}

TEST(QmHudMediaIslandSatellite, SdfSmoothUnionFallsBackToMinimumWhenBlendIsDisabled)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandSdfSmoothUnion(0.4f, -0.2f, 0.0f), -0.2f);
	EXPECT_LT(QmHudMediaIslandSdfSmoothUnion(0.4f, 0.4f, 1.0f), 0.4f);
}

TEST(QmHudMediaIslandSatellite, SdfRoundedRectKeepsMainIslandCornersRounded)
{
	const CUIRect MainIsland = {0.0f, 0.0f, 20.0f, 16.0f};

	EXPECT_LT(QmHudMediaIslandSdfRoundedRect(vec2(10.0f, 8.0f), MainIsland, 8.0f, IGraphics::CORNER_ALL), 0.0f);
	EXPECT_GT(QmHudMediaIslandSdfRoundedRect(vec2(0.0f, 0.0f), MainIsland, 8.0f, IGraphics::CORNER_ALL), 0.0f);
	EXPECT_NEAR(QmHudMediaIslandSdfRoundedRect(vec2(0.0f, 8.0f), MainIsland, 8.0f, IGraphics::CORNER_ALL), 0.0f, 0.0001f);
	EXPECT_NEAR(QmHudMediaIslandSdfRoundedRect(vec2(0.0f, 0.0f), MainIsland, 8.0f, IGraphics::CORNER_R), 0.0f, 0.0001f);
	EXPECT_GT(QmHudMediaIslandSdfRoundedRect(vec2(0.0f, 0.0f), MainIsland, 8.0f, IGraphics::CORNER_NONE, 8.0f), 0.0f);
}

TEST(QmHudMediaIslandSdfBounds, PaddingContainsSmoothUnionAndFeatherOverflow)
{
	constexpr float ScreenPixelSize = 0.5f;
	const float StrongSmoothUnion = QmHudMediaIslandBlobBlend(8.0f, 1.0f);
	const float RequiredOverflow = StrongSmoothUnion * 0.25f + ScreenPixelSize * 0.9f;

	SHudMediaIslandSdfRenderState LeftSatelliteState;
	LeftSatelliteState.m_ItemCount = 1;
	LeftSatelliteState.m_Items[0].m_SmoothUnion = StrongSmoothUnion;
	LeftSatelliteState.m_ScreenPixelSize = ScreenPixelSize;
	EXPECT_GE(QmHudMediaIslandSdfPadding(LeftSatelliteState), RequiredOverflow);

	SHudMediaIslandSdfRenderState RightSatelliteState;
	RightSatelliteState.m_HasRightCapsule = true;
	RightSatelliteState.m_RightCapsule.m_SmoothUnion = StrongSmoothUnion;
	RightSatelliteState.m_ScreenPixelSize = ScreenPixelSize;
	EXPECT_GE(QmHudMediaIslandSdfPadding(RightSatelliteState), RequiredOverflow);

	SHudMediaIslandSdfRenderState RestingState;
	RestingState.m_ScreenPixelSize = ScreenPixelSize;
	EXPECT_FLOAT_EQ(QmHudMediaIslandSdfPadding(RestingState), 1.5f);

	SHudMediaIslandSdfRenderState ShadowState;
	ShadowState.m_ScreenPixelSize = ScreenPixelSize;
	ShadowState.m_OuterShadowSize = 3.0f;
	EXPECT_GE(QmHudMediaIslandSdfPadding(ShadowState), 3.0f + ScreenPixelSize * 0.9f);
}

TEST(QmHudMediaIslandSdfBounds, OuterRectKeepsEveryLiquidEdgeInsideTheQuad)
{
	SHudMediaIslandSdfRenderState State;
	State.m_MainRect = {10.0f, 10.0f, 20.0f, 10.0f};
	State.m_ItemCount = 1;
	State.m_Items[0].m_Center = vec2(4.0f, 15.0f);
	State.m_Items[0].m_Radii = vec2(4.0f, 5.0f);
	State.m_Items[0].m_SmoothUnion = 8.0f;
	State.m_HasRightCapsule = true;
	State.m_RightCapsule.m_Rect = {32.0f, 10.0f, 8.0f, 10.0f};
	State.m_RightCapsule.m_SmoothUnion = 8.0f;
	State.m_ScreenPixelSize = 0.5f;

	const float Padding = QmHudMediaIslandSdfPadding(State);
	const CUIRect OuterRect = QmHudMediaIslandSdfOuterRect(State);
	EXPECT_FLOAT_EQ(OuterRect.x, -Padding);
	EXPECT_FLOAT_EQ(OuterRect.y, 10.0f - Padding);
	EXPECT_FLOAT_EQ(OuterRect.x + OuterRect.w, 40.0f + Padding);
	EXPECT_FLOAT_EQ(OuterRect.y + OuterRect.h, 20.0f + Padding);
}

TEST(QmHudMediaIslandBackdrop, TransparentOpacityIncludesPureBlurAndSkipsOpaqueBackground)
{
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(0, true));
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(1, true));
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(99, true));
	EXPECT_FALSE(QmHudMediaIslandShouldPrepareBackdropBlur(100, true));
	EXPECT_FALSE(QmHudMediaIslandShouldPrepareBackdropBlur(0, false));
	EXPECT_FALSE(QmHudMediaIslandShouldPrepareBackdropBlur(99, false));
}

TEST(QmHudMediaIslandBackdrop, RefreshesBlurOnlyAfterTheShortFrameAttemptInterval)
{
	EXPECT_TRUE(QmHudMediaIslandShouldRefreshBackdropBlur(10, 0, false));
	// 失败尝试也要进入短暂冷却，避免后端持续失败时每帧重试。
	EXPECT_FALSE(QmHudMediaIslandShouldRefreshBackdropBlur(11, 10, true));
	EXPECT_FALSE(QmHudMediaIslandShouldRefreshBackdropBlur(10, 10, true));
	EXPECT_FALSE(QmHudMediaIslandShouldRefreshBackdropBlur(12, 10, true));
	EXPECT_TRUE(QmHudMediaIslandShouldRefreshBackdropBlur(13, 10, true));
	EXPECT_TRUE(QmHudMediaIslandShouldRefreshBackdropBlur(9, 10, true));
}

TEST(QmHudMediaIslandBackdrop, MapsTheAnimatedOuterRectToTheCapturedScreenTexture)
{
	const CUIRect OuterRect = {120.0f, 30.0f, 80.0f, 40.0f};
	const CUIRect ScreenRect = {0.0f, 0.0f, 400.0f, 200.0f};
	const vec4 BackdropUv = QmHudMediaIslandBackdropUv(OuterRect, ScreenRect);

	EXPECT_FLOAT_EQ(BackdropUv.x, 0.3f);
	EXPECT_FLOAT_EQ(BackdropUv.y, 0.85f);
	EXPECT_FLOAT_EQ(BackdropUv.z, 0.2f);
	EXPECT_FLOAT_EQ(BackdropUv.w, -0.2f);
	const vec4 InvalidBackdropUv = QmHudMediaIslandBackdropUv(OuterRect, CUIRect());
	EXPECT_FLOAT_EQ(InvalidBackdropUv.x, 0.0f);
	EXPECT_FLOAT_EQ(InvalidBackdropUv.y, 0.0f);
	EXPECT_FLOAT_EQ(InvalidBackdropUv.z, 0.0f);
	EXPECT_FLOAT_EQ(InvalidBackdropUv.w, 0.0f);
}

TEST(QmHudMediaIslandSdfGpuPacking, CopiesAllShapeAndAnimationInputs)
{
	SHudMediaIslandSdfRenderState State;
	State.m_Rect = {1.0f, 2.0f, 80.0f, 24.0f};
	State.m_MainRect = {10.0f, 2.0f, 50.0f, 20.0f};
	State.m_MainRadius = 10.0f;
	State.m_MainCorners = IGraphics::CORNER_T | IGraphics::CORNER_BR;
	State.m_MainDisabledCornerRadius = 2.0f;
	State.m_ItemCount = 1;
	State.m_Items[0].m_Center = vec2(5.0f, 12.0f);
	State.m_Items[0].m_Radii = vec2(8.0f, 9.0f);
	State.m_Items[0].m_SmoothUnion = 3.0f;
	State.m_Items[0].m_ContentAlpha = 0.8f;
	State.m_Items[0].m_ContentScale = 0.7f;
	State.m_Items[0].m_CountdownProgress = 0.6f;
	State.m_Items[0].m_RingColor = ColorRGBA(0.1f, 0.9f, 1.0f, 0.7f);
	State.m_HasRightCapsule = true;
	State.m_RightCapsule.m_Rect = {62.0f, 2.0f, 20.0f, 20.0f};
	State.m_RightCapsule.m_Radius = 10.0f;
	State.m_RightCapsule.m_SmoothUnion = 4.0f;
	State.m_RingRadius = 6.0f;
	State.m_RingThickness = 1.5f;
	State.m_BackgroundColor = ColorRGBA(0.02f, 0.03f, 0.05f, 0.9f);
	State.m_ScreenPixelSize = 0.5f;
	State.m_OuterShadowSize = 1.0f;
	State.m_OuterShadowOpacity = 0.14f;
	State.m_BackdropUv = vec4(0.1f, 0.9f, 0.2f, -0.3f);

	IGraphics::SMediaIslandSdfParams Params;
	ASSERT_TRUE(QmHudMediaIslandBuildGpuSdfParams(State, Params));
	EXPECT_EQ(Params.ItemCount(), 1);
	EXPECT_TRUE(Params.HasRightCapsule());
	EXPECT_EQ(Params.MainCorners(), State.m_MainCorners);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RECT].z, 80.0f);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RESERVED].x, 1.0f);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RESERVED].y, 0.14f);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RESERVED].z, 0.0f);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_RESERVED].w, 0.0f);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV].x, State.m_BackdropUv.x);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV].y, State.m_BackdropUv.y);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV].z, State.m_BackdropUv.z);
	EXPECT_FLOAT_EQ(Params.m_aData[IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV].w, State.m_BackdropUv.w);
	EXPECT_FLOAT_EQ(Params.Item(0, 0).z, 8.0f);
	EXPECT_FLOAT_EQ(Params.Item(0, 1).w, 0.6f);
	EXPECT_FLOAT_EQ(Params.Item(0, 2).g, 0.9f);
	EXPECT_FLOAT_EQ(Params.Item(0, 2).a, 0.7f);
}

TEST(QmHudMediaIslandSatellite, ParsesOwnSpamProtectionMuteOnly)
{
	int Seconds = 0;
	EXPECT_EQ(QmHudParseSpamProtectionMute("'Main' has been muted for 60 seconds (Spam protection)", "Main", "Dummy", Seconds), EHudMediaIslandMuteMessage::SPAM_BROADCAST);
	EXPECT_EQ(Seconds, 60);
	EXPECT_EQ(QmHudParseSpamProtectionMute("'Other' has been muted for 60 seconds (Spam protection)", "Main", "Dummy", Seconds), EHudMediaIslandMuteMessage::NONE);
	EXPECT_EQ(QmHudParseSpamProtectionMute("'Main' has been muted for 60 seconds (manual)", "Main", "Dummy", Seconds), EHudMediaIslandMuteMessage::NONE);
	EXPECT_EQ(QmHudParseSpamProtectionMute("'O'Brien' has been muted for 45 seconds (Spam protection)", "O'Brien", "Dummy", Seconds), EHudMediaIslandMuteMessage::SPAM_BROADCAST);
	EXPECT_EQ(Seconds, 45);
}

TEST(QmHudMediaIslandSatellite, ParsesActiveMuteRemainingMessageSeparately)
{
	int Seconds = 0;
	EXPECT_EQ(QmHudParseSpamProtectionMute("You are not permitted to talk for the next 17 seconds.", "Main", "Dummy", Seconds), EHudMediaIslandMuteMessage::REMAINING);
	EXPECT_EQ(Seconds, 17);
	EXPECT_EQ(QmHudParseSpamProtectionMute("This server has an initial chat delay, you will be able to talk in 17 seconds.", "Main", "Dummy", Seconds), EHudMediaIslandMuteMessage::NONE);
}

TEST(QmHudMediaIslandTimerLayout, SecondaryLinePreservesTenPercentTopMargin)
{
	const SHudMediaIslandTimerRowLayout Layout = QmHudMediaIslandTimerRows(1.0f, 16.0f, true);

	EXPECT_FLOAT_EQ(Layout.m_RaceY, 2.6f);
	EXPECT_FLOAT_EQ(Layout.m_RaceH, 9.6f);
	EXPECT_FLOAT_EQ(Layout.m_CheckpointY, 12.2f);
	EXPECT_FLOAT_EQ(Layout.m_CheckpointH, 4.8f);
}

TEST(QmHudMediaIslandTimerLayout, RaceUsesTheWholeSlotWithoutSecondaryLine)
{
	const SHudMediaIslandTimerRowLayout Layout = QmHudMediaIslandTimerRows(1.0f, 16.0f, false);

	EXPECT_FLOAT_EQ(Layout.m_RaceY, 1.0f);
	EXPECT_FLOAT_EQ(Layout.m_RaceH, 16.0f);
	EXPECT_FLOAT_EQ(Layout.m_CheckpointH, 0.0f);
}

TEST(QmHudMediaIslandWaveform, PlayingBarsVaryIndependentlyAndPausedBarsSettle)
{
	bool AnyChanged = false;
	for(int Bar = 0; Bar < 7; ++Bar)
	{
		const float First = QmHudMediaIslandWaveBarHeight(Bar, 1.0f, 1.0f);
		const float Next = QmHudMediaIslandWaveBarHeight(Bar, 1.1f, 1.0f);
		EXPECT_GE(First, 0.20f);
		EXPECT_LE(First, 1.0f);
		EXPECT_FLOAT_EQ(QmHudMediaIslandWaveBarHeight(Bar, 1.0f, 0.0f), 0.20f);
		EXPECT_FLOAT_EQ(QmHudMediaIslandWaveBarHeight(Bar, 1.0f, 0.5f), 0.20f + (First - 0.20f) * 0.5f);
		AnyChanged |= std::abs(First - Next) > 0.001f;
	}
	EXPECT_TRUE(AnyChanged);

	constexpr int SampleCount = 80;
	constexpr float SampleStep = 0.125f;
	for(int FirstBar = 0; FirstBar < 7; ++FirstBar)
	{
		for(int SecondBar = FirstBar + 1; SecondBar < 7; ++SecondBar)
		{
			double FirstSum = 0.0;
			double SecondSum = 0.0;
			double FirstSquaredSum = 0.0;
			double SecondSquaredSum = 0.0;
			double ProductSum = 0.0;
			for(int Sample = 0; Sample < SampleCount; ++Sample)
			{
				const float Time = Sample * SampleStep;
				const double First = QmHudMediaIslandWaveBarHeight(FirstBar, Time, 1.0f);
				const double Second = QmHudMediaIslandWaveBarHeight(SecondBar, Time, 1.0f);
				FirstSum += First;
				SecondSum += Second;
				FirstSquaredSum += First * First;
				SecondSquaredSum += Second * Second;
				ProductSum += First * Second;
			}

			const double Numerator = SampleCount * ProductSum - FirstSum * SecondSum;
			const double Denominator = std::sqrt(
				(SampleCount * FirstSquaredSum - FirstSum * FirstSum) *
				(SampleCount * SecondSquaredSum - SecondSum * SecondSum));
			ASSERT_GT(Denominator, 0.0);
			EXPECT_LT(std::abs(Numerator / Denominator), 0.20) << "bars " << FirstBar << " and " << SecondBar;
		}
	}
}

TEST(QmHudMediaIslandWaveform, StoppedBarsSettleFromOutsideIn)
{
	constexpr int BarCount = 7;
	constexpr float MidSettleTime = 0.40f;
	const float Outer = QmHudMediaIslandWaveBarSettleProgress(0, BarCount, MidSettleTime);
	const float NextOuter = QmHudMediaIslandWaveBarSettleProgress(1, BarCount, MidSettleTime);
	const float NextInner = QmHudMediaIslandWaveBarSettleProgress(2, BarCount, MidSettleTime);
	const float Center = QmHudMediaIslandWaveBarSettleProgress(3, BarCount, MidSettleTime);

	EXPECT_GT(Outer, NextOuter);
	EXPECT_GT(NextOuter, NextInner);
	EXPECT_GT(NextInner, Center);
	EXPECT_FLOAT_EQ(Outer, QmHudMediaIslandWaveBarSettleProgress(6, BarCount, MidSettleTime));
	EXPECT_FLOAT_EQ(NextOuter, QmHudMediaIslandWaveBarSettleProgress(5, BarCount, MidSettleTime));
	EXPECT_FLOAT_EQ(NextInner, QmHudMediaIslandWaveBarSettleProgress(4, BarCount, MidSettleTime));
	for(int Bar = 0; Bar < BarCount; ++Bar)
	{
		EXPECT_FLOAT_EQ(QmHudMediaIslandWaveBarSettleProgress(Bar, BarCount, -0.1f), 0.0f);
		EXPECT_FLOAT_EQ(QmHudMediaIslandWaveBarSettleProgress(Bar, BarCount, 0.9f), 1.0f);
	}
}
