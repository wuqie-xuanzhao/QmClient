// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <game/client/components/hud_media_island_logic.h>

#include <gtest/gtest.h>

#include <array>

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
	const CUIRect Island = {120.0f, 1.0f, 80.0f, 38.0f};

	EXPECT_FLOAT_EQ(QmHudTopEffectY(20.0f, 10.0f, 140.0f, 180.0f, Island, true), 42.0f);
}

TEST(QmHudMediaIslandLogic, TopEffectDoesNotMoveForHorizontalSeparationOrHiddenIsland)
{
	const CUIRect SideIsland = {20.0f, 1.0f, 60.0f, 38.0f};
	const CUIRect CenterIsland = {120.0f, 1.0f, 80.0f, 38.0f};

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

	EXPECT_FLOAT_EQ(Pose.m_Rect.x, 132.0f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.y, 9.0f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.w, 16.0f);
	EXPECT_FLOAT_EQ(Pose.m_Rect.h, 16.0f);
	EXPECT_FLOAT_EQ(Pose.m_Radius, 8.0f);
	EXPECT_FLOAT_EQ(Pose.m_DisabledCornerRadius, 8.0f);
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
	EXPECT_LE(FirstVisibleContent.m_Rect.x, WideTarget.x + 2.0f);
	EXPECT_GE(FirstVisibleContent.m_Rect.x + FirstVisibleContent.m_Rect.w, WideTarget.x + WideTarget.w - 2.0f);
}

TEST(QmHudMediaIslandEntrance, IntermediatePoseMorphsGeometryAndConfiguredBackgroundTogether)
{
	const CUIRect Target = {100.0f, 1.0f, 80.0f, 32.0f};
	const ColorRGBA TargetColor(0.25f, 0.50f, 0.75f, 0.60f);

	const SHudMediaIslandEntrancePose Pose = QmHudMediaIslandEntrancePose(Target, 8.0f, TargetColor, 0.5f);

	EXPECT_GT(Pose.m_Rect.w, 16.0f);
	EXPECT_LT(Pose.m_Rect.w, Target.w);
	EXPECT_GT(Pose.m_Rect.h, 16.0f);
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

TEST(QmHudMediaIslandSatellite, KeepsLatestVisibleSwitchesAndSeparatesTeamIdentity)
{
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(5, 3), 2);
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(3, 3), 0);
	EXPECT_EQ(QmHudMediaIslandVisibleSuffixStart(2, 0), 2);
	EXPECT_NE(QmHudMediaIslandSwitchInstanceId(1, 7), QmHudMediaIslandSwitchInstanceId(2, 7));
	EXPECT_EQ(QmHudMediaIslandSwitchInstanceId(2, 7) & 0xff, 7);
}

TEST(QmHudMediaIslandSatellite, LiquidPoseTraversesFiveOrderedPhases)
{
	EXPECT_EQ(QmHudMediaIslandLiquidPose(0.08f).m_Phase, EHudMediaIslandLiquidPhase::BULGE);
	EXPECT_EQ(QmHudMediaIslandLiquidPose(0.25f).m_Phase, EHudMediaIslandLiquidPhase::FORM);
	EXPECT_EQ(QmHudMediaIslandLiquidPose(0.50f).m_Phase, EHudMediaIslandLiquidPhase::STRETCH);
	EXPECT_EQ(QmHudMediaIslandLiquidPose(0.72f).m_Phase, EHudMediaIslandLiquidPhase::BREAK);
	EXPECT_EQ(QmHudMediaIslandLiquidPose(0.90f).m_Phase, EHudMediaIslandLiquidPhase::REBOUND);
}

TEST(QmHudMediaIslandSatellite, LiquidPoseBulgesFormsStretchesBreaksAndSettles)
{
	const SHudMediaIslandLiquidPose Hidden = QmHudMediaIslandLiquidPose(0.0f);
	EXPECT_FLOAT_EQ(Hidden.m_RadiusScale, 0.0f);
	EXPECT_FLOAT_EQ(Hidden.m_SmoothUnionScale, 0.0f);
	EXPECT_FLOAT_EQ(Hidden.m_ContentAlpha, 0.0f);

	const SHudMediaIslandLiquidPose Bulge = QmHudMediaIslandLiquidPose(0.16f);
	EXPECT_NEAR(Bulge.m_CenterProgress, 0.0f, 0.05f);
	EXPECT_GT(Bulge.m_RadiusScale, 0.2f);
	EXPECT_GT(Bulge.m_SmoothUnionScale, 0.75f);
	EXPECT_FLOAT_EQ(Bulge.m_ContentAlpha, 0.0f);

	const SHudMediaIslandLiquidPose Form = QmHudMediaIslandLiquidPose(0.37f);
	EXPECT_GT(Form.m_RadiusScale, 0.9f);
	EXPECT_GT(Form.m_SmoothUnionScale, 0.75f);
	EXPECT_LT(Form.m_CenterProgress, 0.35f);

	const SHudMediaIslandLiquidPose Stretch = QmHudMediaIslandLiquidPose(0.65f);
	EXPECT_GT(Stretch.m_StretchX, 1.15f);
	EXPECT_LT(Stretch.m_StretchY, 0.95f);
	EXPECT_GT(Stretch.m_SmoothUnionScale, 0.0f);
	EXPECT_GT(Stretch.m_CenterProgress, 0.7f);

	const SHudMediaIslandLiquidPose Broken = QmHudMediaIslandLiquidPose(0.79f);
	EXPECT_FLOAT_EQ(Broken.m_SmoothUnionScale, 0.0f);
	EXPECT_GT(Broken.m_CenterProgress, 1.0f);

	const SHudMediaIslandLiquidPose Settled = QmHudMediaIslandLiquidPose(1.0f);
	EXPECT_FLOAT_EQ(Settled.m_CenterProgress, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_RadiusScale, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_StretchX, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_StretchY, 1.0f);
	EXPECT_FLOAT_EQ(Settled.m_SmoothUnionScale, 0.0f);
	EXPECT_FLOAT_EQ(Settled.m_ContentAlpha, 1.0f);
}

TEST(QmHudMediaIslandSatellite, LiquidProgressReversesWithoutResettingPose)
{
	float Progress = QmHudAdvanceMediaIslandLiquidProgress(0.0f, true, 0.28f, true);
	EXPECT_NEAR(Progress, 0.5f, 0.0001f);
	const SHudMediaIslandLiquidPose BeforeReverse = QmHudMediaIslandLiquidPose(Progress);

	Progress = QmHudAdvanceMediaIslandLiquidProgress(Progress, false, 0.14f, true);
	EXPECT_NEAR(Progress, 0.25f, 0.0001f);
	Progress = QmHudAdvanceMediaIslandLiquidProgress(Progress, true, 0.14f, true);
	EXPECT_NEAR(Progress, 0.5f, 0.0001f);

	const SHudMediaIslandLiquidPose AfterReverse = QmHudMediaIslandLiquidPose(Progress);
	EXPECT_EQ(AfterReverse.m_Phase, BeforeReverse.m_Phase);
	EXPECT_NEAR(AfterReverse.m_CenterProgress, BeforeReverse.m_CenterProgress, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_RadiusScale, BeforeReverse.m_RadiusScale, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_StretchX, BeforeReverse.m_StretchX, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_StretchY, BeforeReverse.m_StretchY, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_SmoothUnionScale, BeforeReverse.m_SmoothUnionScale, 0.0001f);
	EXPECT_NEAR(AfterReverse.m_ContentAlpha, BeforeReverse.m_ContentAlpha, 0.0001f);
}

TEST(QmHudMediaIslandSatellite, LiquidProgressClampsAndReducedMotionSnaps)
{
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.95f, true, 1.0f, true), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.05f, false, 1.0f, true), 0.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, true, -1.0f, true), 0.4f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, true, 0.01f, false), 1.0f);
	EXPECT_FLOAT_EQ(QmHudAdvanceMediaIslandLiquidProgress(0.4f, false, 0.01f, false), 0.0f);
}

TEST(QmHudMediaIslandSatellite, RightLiquidCapsuleSettlesOutsideMainIsland)
{
	const SHudMediaIslandLiquidCapsule Capsule = QmHudMediaIslandRightLiquidCapsule(100.0f, 20.0f, 8.0f, 24.0f, 4.0f, QmHudMediaIslandLiquidPose(1.0f));

	EXPECT_FLOAT_EQ(Capsule.m_Rect.x, 104.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.y, 12.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.w, 24.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Rect.h, 16.0f);
	EXPECT_FLOAT_EQ(Capsule.m_Radius, 8.0f);
	EXPECT_FLOAT_EQ(Capsule.m_SmoothUnion, 0.0f);
	EXPECT_FLOAT_EQ(Capsule.m_ContentAlpha, 1.0f);
}

TEST(QmHudMediaIslandSatellite, RightLiquidCapsuleStretchesWhileStillConnected)
{
	const SHudMediaIslandLiquidCapsule Capsule = QmHudMediaIslandRightLiquidCapsule(100.0f, 20.0f, 8.0f, 24.0f, 4.0f, QmHudMediaIslandLiquidPose(0.65f));

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

TEST(QmHudMediaIslandSatellite, RenderPathUsesLiquidSatelliteInsteadOfCountdownText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const std::string TClientSource = ReadTestSourceFile("src/game/client/components/tclient/tclient.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("void CHud::RenderPlayerState", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("DrawMediaIslandSdf"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudAdvanceMediaIslandLiquidProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandLiquidPose"), std::string::npos);
	EXPECT_NE(RenderBody.find("const float SatelliteRadius = Radius;"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr float SatelliteItemGap = 3.0f;"), std::string::npos);
	EXPECT_NE(RenderBody.find("SatelliteItemPitch = SatelliteDiameter + SatelliteItemGap"), std::string::npos);
	EXPECT_NE(RenderBody.find("const CUIRect MainIslandSdfRect"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandRightLiquidCapsule"), std::string::npos);
	EXPECT_NE(RenderBody.find("EQmIcon::EYE"), std::string::npos);
	EXPECT_NE(Source.find("EQmIcon::SATELLITE_CHECK"), std::string::npos);
	EXPECT_EQ(RenderBody.find("FontIcons::FONT_ICON_EYE"), std::string::npos);
	EXPECT_EQ(RenderBody.find("if(SatelliteRenderItemCount > 0)"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandLiquidBridge"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandProgressRing"), std::string::npos);
	EXPECT_EQ(RenderBody.find("Graphics()->DrawRect(IslandX, IslandY"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_NE(RenderBody.find("const ColorRGBA IconColor = Item.m_Completed ? ColorRGBA(0.20f, 1.0f, 0.42f"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon(Item.m_Type, Item.m_Completed, Item.m_SwapOutgoing)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandSwapVisibleForConnection(Dummy, g_Config.m_ClDummy)"), std::string::npos);
	EXPECT_NE(Source.find("IsSwapCountdownOutgoing(Dummy)"), std::string::npos);
	EXPECT_NE(Source.find("SwapOutgoing ? EQmIcon::SATELLITE_SWAP_OUTGOING : EQmIcon::SATELLITE_SWAP_INCOMING"), std::string::npos);
	EXPECT_NE(RenderBody.find("SdfItem.m_RingColor = MediaIslandCountdownColor(Item.m_Type);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("MediaIslandCountdownColor(Item.m_Type, Item.m_Completed)"), std::string::npos);
	EXPECT_NE(RenderBody.find("SatelliteVisibleLeft"), std::string::npos);
	EXPECT_EQ(RenderBody.find("BuildSwitchCountdownSummary"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_Hud.HandleSpamProtectionMessage(pMsg->m_pMessage);"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_TClient.HandleSwapCountdownMessage(pMsg->m_pMessage, Conn);"), std::string::npos);
	EXPECT_NE(TClientSource.find("Outgoing == m_aSwapCountdownOutgoing[Dummy]"), std::string::npos);
}

TEST(QmHudMediaIslandSatellite, CompletedSwapUsesCheckIconWithoutBottomReadyText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("void CHud::RenderPlayerState", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon(Item.m_Type, Item.m_Completed, Item.m_SwapOutgoing)"), std::string::npos);
	EXPECT_EQ(RenderBody.find("ShowSwapReady"), std::string::npos);
	EXPECT_EQ(RenderBody.find("SwapReadyCount"), std::string::npos);
	EXPECT_EQ(RenderBody.find("SwapBottomContentWidth"), std::string::npos);
	EXPECT_EQ(RenderBody.find("RenderBottomTextCentered(BottomTextY, SwapList"), std::string::npos);
}

TEST(QmHudMediaIslandSource, RenderPathKeepsStableNodesAndEditorRect)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string AnimResolveSource = ReadTestSourceFile("src/game/client/QmUi/QmAnimResolve.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("float CHud::RenderLegacyMediaInfoAt", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"cover_in\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"cover_out\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"track_title_in\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"track_title_out\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"track_meta_in\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("HudMediaIslandNodeKey(\"track_meta_out\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("StartCapsuleMorph(Now)"), std::string::npos);
	EXPECT_NE(RenderBody.find("m_CapsuleMorphNeedsCapture"), std::string::npos);
	EXPECT_NE(RenderBody.find("MorphCompressSec"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudAdvanceMediaIslandEntranceProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntranceDeltaSeconds, MotionLevel"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandEntrancePose"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_BackgroundColor"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_ContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_DisabledCornerRadius"), std::string::npos);
	EXPECT_NE(RenderBody.find("CoverInAlpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("TrackTitleInAlpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("TimerCapsule.m_Alpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("RenderMediaIslandLine(LyricsRect, BottomFontSize, VisibleBottomAlpha)"), std::string::npos);
	EXPECT_NE(RenderBody.find("0.42f * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("SdfItem.m_ContentScale = Item.m_ContentScale * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("SatelliteIconSize * Item.m_ContentScale * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(AnimResolveSource.find("Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;"), std::string::npos);
	EXPECT_EQ(RenderBody.find("EUiAnimInterruptPolicy::QUEUE"), std::string::npos);
	EXPECT_EQ(RenderBody.find("m_CoverRotation"), std::string::npos);
	EXPECT_NE(RenderBody.find("m_MediaIslandLastVisibleRect = HudEditorScope.m_VisibleRect;"), std::string::npos);
	EXPECT_NE(RenderBody.find("BeginTransform(EHudEditorElement::MediaIsland, EditorTransformRect, EditorVisibleRect"), std::string::npos);
}

TEST(QmHudMediaIslandSource, IslandRendersBeforeCheckpointAndFinishEffects)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const size_t OnRenderBegin = Source.find("void CHud::OnRender()");
	ASSERT_NE(OnRenderBegin, std::string::npos);
	const size_t OnRenderEnd = Source.find("void CHud::OnMessage", OnRenderBegin);
	ASSERT_NE(OnRenderEnd, std::string::npos);
	const std::string OnRenderBody = Source.substr(OnRenderBegin, OnRenderEnd - OnRenderBegin);

	const size_t IslandRender = OnRenderBody.find("RenderMediaIsland();");
	const size_t EffectsRender = OnRenderBody.find("RenderDDRaceEffects();");
	ASSERT_NE(IslandRender, std::string::npos);
	ASSERT_NE(EffectsRender, std::string::npos);
	EXPECT_LT(IslandRender, EffectsRender);
}

TEST(QmHudPresentationSource, MediaIslandAndWeaponHudUseContinuousPresentationState)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string Header = ReadTestSourceFile("src/game/client/components/hud.h");

	EXPECT_NE(Source.find("#include <game/client/QmUi/QmAnimResolve.h>"), std::string::npos);
	EXPECT_EQ(Source.find("float ResolvePresentationStateValue("), std::string::npos);

	const size_t IslandBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(IslandBegin, std::string::npos);
	const size_t IslandEnd = Source.find("float CHud::RenderLegacyMediaInfoAt", IslandBegin);
	ASSERT_NE(IslandEnd, std::string::npos);
	const std::string IslandBody = Source.substr(IslandBegin, IslandEnd - IslandBegin);
	EXPECT_NE(IslandBody.find("ResolveUiPresentationStateValue(AnimRuntime, CapsuleNode"), std::string::npos);
	EXPECT_NE(IslandBody.find("ResolveUiPresentationStateValue(AnimRuntime, CoverInNode"), std::string::npos);
	EXPECT_NE(IslandBody.find("BuildContentSpring(false)"), std::string::npos);
	EXPECT_NE(IslandBody.find("TrackExitSpring"), std::string::npos);
	EXPECT_NE(IslandBody.find("ExitTimeScale"), std::string::npos);
	EXPECT_EQ(IslandBody.find("EUiAnimInterruptPolicy::QUEUE"), std::string::npos);

	const size_t PlayerStateBegin = Source.find("void CHud::RenderPlayerState");
	ASSERT_NE(PlayerStateBegin, std::string::npos);
	const size_t PlayerStateEnd = Source.find("void CHud::RenderNinjaBarPos", PlayerStateBegin);
	ASSERT_NE(PlayerStateEnd, std::string::npos);
	const std::string PlayerStateBody = Source.substr(PlayerStateBegin, PlayerStateEnd - PlayerStateBegin);
	EXPECT_NE(Source.find("HudWeaponPresentationNodeKey"), std::string::npos);
	EXPECT_NE(Header.find("SHudWeaponPresentationState"), std::string::npos);
	EXPECT_NE(PlayerStateBody.find("ResolveUiPresentationStateValue(AnimRuntime, WeaponNode"), std::string::npos);
	EXPECT_EQ(Source.find("m_aHudWeaponSwitchStartTimes"), std::string::npos);
	EXPECT_EQ(Source.find("HudActiveWeaponSwitchScale"), std::string::npos);
}
