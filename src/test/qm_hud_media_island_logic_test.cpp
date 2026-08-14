// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include "test.h"

#include <engine/graphics.h>

#include <game/client/components/hud_media_island_logic.h>
#include <game/client/components/qmclient/tune_zone_effects.h>
#include <game/client/components/tclient/pet.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace
{
	std::string FunctionBody(const std::string &Source, const std::string &Signature)
	{
		const size_t FunctionStart = Source.find(Signature);
		if(FunctionStart == std::string::npos)
			return {};
		const size_t BodyStart = Source.find('{', FunctionStart);
		if(BodyStart == std::string::npos)
			return {};
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}' && --Depth == 0)
				return Source.substr(BodyStart, Index - BodyStart);
		}
		return {};
	}

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

TEST(QmTuneZoneEffects, MapsEveryEffectiveTuningParameterToItsCategory)
{
	using ECategory = EQmTuneZoneEffectCategory;
	static_assert(sizeof(CTuningParams) == sizeof(int) * 47);
	constexpr std::array<ECategory, 47> aExpectedCategories = {
		ECategory::MOVEMENT,
		ECategory::MOVEMENT,
		ECategory::MOVEMENT,
		ECategory::JUMP,
		ECategory::JUMP,
		ECategory::MOVEMENT,
		ECategory::MOVEMENT,
		ECategory::MOVEMENT,
		ECategory::HOOK,
		ECategory::HOOK,
		ECategory::HOOK,
		ECategory::HOOK,
		ECategory::GRAVITY,
		ECategory::VELRAMP,
		ECategory::VELRAMP,
		ECategory::VELRAMP,
		ECategory::GUN_JETPACK,
		ECategory::GUN_JETPACK,
		ECategory::GUN_JETPACK,
		ECategory::SHOTGUN,
		ECategory::SHOTGUN,
		ECategory::UNUSED,
		ECategory::UNUSED,
		ECategory::GRENADE_EXPLOSION,
		ECategory::GRENADE_EXPLOSION,
		ECategory::GRENADE_EXPLOSION,
		ECategory::LASER,
		ECategory::LASER,
		ECategory::LASER,
		ECategory::LASER,
		ECategory::UNUSED,
		ECategory::COLLISION,
		ECategory::COLLISION,
		ECategory::GUN_JETPACK,
		ECategory::SHOTGUN,
		ECategory::GRENADE_EXPLOSION,
		ECategory::HAMMER,
		ECategory::HOOK,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::WEAPON_FIRE_RATE,
		ECategory::ELASTICITY,
		ECategory::ELASTICITY,
	};

	const CTuningParams ZoneZero;
	for(int Parameter = 0; Parameter < CTuningParams::Num(); ++Parameter)
	{
		CTuningParams Zone = ZoneZero;
		float Value = 0.0f;
		ASSERT_TRUE(Zone.Get(Parameter, &Value));
		ASSERT_TRUE(Zone.Set(Parameter, Value + 1.0f));

		const SQmTuneZoneEffectSummary Summary = BuildQmTuneZoneEffectSummary(ZoneZero, Zone);
		if(aExpectedCategories[Parameter] == ECategory::UNUSED)
		{
			EXPECT_EQ(Summary.m_Count, 0) << CTuningParams::Name(Parameter);
		}
		else
		{
			ASSERT_EQ(Summary.m_Count, 1) << CTuningParams::Name(Parameter);
			EXPECT_EQ(Summary.m_aCategories[0], aExpectedCategories[Parameter]) << CTuningParams::Name(Parameter);
		}
	}
}

TEST(QmTuneZoneEffects, DeduplicatesCategoriesAndUsesFixedPriority)
{
	CTuningParams ZoneZero;
	CTuningParams Zone = ZoneZero;
	Zone.m_Gravity = 0.75f;
	Zone.m_GroundControlSpeed = 12.0f;
	Zone.m_AirFriction = 0.80f;
	Zone.m_GroundJumpImpulse = 15.0f;
	Zone.m_HookLength = 420.0f;
	Zone.m_PlayerCollision = 0;
	Zone.m_JetpackStrength = 500.0f;
	Zone.m_ShotgunStrength = 12.0f;
	Zone.m_ExplosionStrength = 8.0f;
	Zone.m_LaserReach = 900.0f;
	Zone.m_HammerStrength = 2.0f;
	Zone.m_GunFireDelay = 100;
	Zone.m_VelrampStart = 600.0f;
	Zone.m_GroundElasticityX = 0.5f;

	const SQmTuneZoneEffectSummary Summary = BuildQmTuneZoneEffectSummary(ZoneZero, Zone);
	const std::array<EQmTuneZoneEffectCategory, 13> aExpected = {
		EQmTuneZoneEffectCategory::GRAVITY,
		EQmTuneZoneEffectCategory::MOVEMENT,
		EQmTuneZoneEffectCategory::JUMP,
		EQmTuneZoneEffectCategory::HOOK,
		EQmTuneZoneEffectCategory::COLLISION,
		EQmTuneZoneEffectCategory::GUN_JETPACK,
		EQmTuneZoneEffectCategory::SHOTGUN,
		EQmTuneZoneEffectCategory::GRENADE_EXPLOSION,
		EQmTuneZoneEffectCategory::LASER,
		EQmTuneZoneEffectCategory::HAMMER,
		EQmTuneZoneEffectCategory::WEAPON_FIRE_RATE,
		EQmTuneZoneEffectCategory::VELRAMP,
		EQmTuneZoneEffectCategory::ELASTICITY,
	};

	ASSERT_EQ(Summary.m_Count, (int)aExpected.size());
	EXPECT_EQ(Summary.m_aCategories, aExpected);
	EXPECT_EQ(Summary.VisibleCategoryCount(), 7);
	EXPECT_EQ(Summary.HiddenCategoryCount(), 6);
	EXPECT_EQ(Summary.DisplaySlotCount(), 8);
}

TEST(QmTuneZoneEffects, IdenticalZoneHasNoSatelliteSlots)
{
	const CTuningParams ZoneZero;
	const SQmTuneZoneEffectSummary Summary = BuildQmTuneZoneEffectSummary(ZoneZero, ZoneZero);

	EXPECT_FALSE(Summary.HasEffects());
	EXPECT_EQ(Summary.VisibleCategoryCount(), 0);
	EXPECT_EQ(Summary.HiddenCategoryCount(), 0);
	EXPECT_EQ(Summary.DisplaySlotCount(), 0);
}

TEST(QmTuneZoneEffects, SatelliteWidthGrowsWithVisibleSlotsAndOverflowSlot)
{
	SQmTuneZoneEffectSummary Summary;
	EXPECT_FLOAT_EQ(QmTuneZoneEffectSatelliteWidth(Summary, 16.0f, 6.0f, 2.0f, 3.0f), 0.0f);

	Summary.m_Count = 1;
	EXPECT_FLOAT_EQ(QmTuneZoneEffectSatelliteWidth(Summary, 16.0f, 6.0f, 2.0f, 3.0f), 16.0f);

	Summary.m_Count = 7;
	EXPECT_FLOAT_EQ(QmTuneZoneEffectSatelliteWidth(Summary, 16.0f, 6.0f, 2.0f, 3.0f), 60.0f);

	Summary.m_Count = 13;
	EXPECT_FLOAT_EQ(QmTuneZoneEffectSatelliteWidth(Summary, 16.0f, 6.0f, 2.0f, 3.0f), 68.0f);
}

TEST(QmTuneZoneEffectsSource, HudUsesCurrentNonzeroZoneAndReusesExistingSatellite)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string Header = ReadTestSourceFile("src/game/client/components/hud.h");
	const std::string Config = ReadTestSourceFile("src/engine/shared/config_variables_qmclient.h");

	EXPECT_NE(Source.find("TuneZone <= 0 || TuneZone >= NUM_TUNEZONES"), std::string::npos);
	EXPECT_NE(Source.find("BuildQmTuneZoneEffectSummary(*GameClient.GetTuning(0), *GameClient.GetTuning(TuneZone))"), std::string::npos);
	EXPECT_NE(Source.find("EHudMediaIslandCountdownType::TUNE_ZONE"), std::string::npos);
	EXPECT_NE(Source.find("Item.m_TuneZoneSummary"), std::string::npos);
	EXPECT_NE(Source.find("SdfItem.m_Radii = Item.m_Radii * EntranceContentAlpha"), std::string::npos);
	EXPECT_EQ(Source.find("QmHudMediaIslandLeftBlobCapsule"), std::string::npos);
	EXPECT_EQ(Header.find("m_TuneZoneLiquidProgress"), std::string::npos);
	EXPECT_NE(Source.find("str_format(aOverflowBuf, sizeof(aOverflowBuf), \"+%d\", HiddenCategoryCount)"), std::string::npos);
	EXPECT_NE(Config.find("QmHudIslandShowTuneZoneEffects, qm_hud_island_show_tune_zone_effects, 1"), std::string::npos);
}

TEST(QmTuneZoneEffectsSource, DynamicIslandUsesCompactSharedSpacing)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");

	EXPECT_NE(Source.find("QmHudMediaIslandScaled(2.0f)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandScaled(3.0f)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandScaled(7.0f)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandScaled(5.0f)"), std::string::npos);
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

TEST(QmTuneZoneEffectsSource, SettingsExposeIconLegend)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/qmclient/menus_qmclient.cpp");

	EXPECT_NE(Source.find("Tune Zone effect icon legend"), std::string::npos);
	EXPECT_NE(Source.find("EQmIcon::TUNE_GRAVITY"), std::string::npos);
	EXPECT_NE(Source.find("EQmIcon::TUNE_ELASTICITY"), std::string::npos);
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

TEST(QmHudMediaIslandLayout, LyricsNeverWidenAnExistingTopRow)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, false, 0.0f, 72.0f, 300.0f, 10.0f), 0.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, false, 0.0f, 500.0f, 300.0f, 10.0f), 0.0f);
}

TEST(QmHudMediaIslandLayout, LyricsOnlyUsesFixedTitleAreaWidth)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, false, false, 0.0f, 72.0f, 300.0f, 10.0f), 92.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, false, false, 0.0f, 500.0f, 100.0f, 10.0f), 100.0f);
}

TEST(QmHudMediaIslandLayout, UtilityBottomContentStillControlsRequestedWidth)
{
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(true, true, true, 45.0f, 72.0f, 300.0f, 10.0f), 65.0f);
	EXPECT_FLOAT_EQ(QmHudMediaIslandDesiredBottomWidth(false, false, true, 45.0f, 72.0f, 300.0f, 10.0f), 65.0f);
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

TEST(QmHudSwitchCountdownSource, FollowRingsReuseMediaIslandSatelliteStyleWithoutIconsOrText)
{
	const std::string HudSource = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string PetSource = ReadTestSourceFile("src/game/client/components/tclient/pet.cpp");
	const std::string FollowBody = FunctionBody(HudSource, "void CHud::RenderFollowSwitchCountdowns()");
	const std::string SatelliteBody = FunctionBody(HudSource, "void DrawMediaIslandCountdownSatellite(");

	EXPECT_NE(PetSource.find("QmTClientPetAdvanceSpring"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmTClientPetAdvanceSpring"), std::string::npos);
	EXPECT_NE(FollowBody.find("GameClient()->m_Pet.IsVisibleForClient"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmHudSwitchCountdownFollowSide"), std::string::npos);
	EXPECT_NE(FollowBody.find("DrawMediaIslandCountdownSatellite"), std::string::npos);
	EXPECT_NE(FollowBody.find("constexpr float SatelliteRadius = 9.0f + 2.5f * 0.5f"), std::string::npos);
	EXPECT_NE(FollowBody.find("RingRadius = SatelliteRadius * MEDIA_ISLAND_SATELLITE_RING_RADIUS_SCALE"), std::string::npos);
	EXPECT_NE(FollowBody.find("SatelliteRadius * MEDIA_ISLAND_SATELLITE_RING_THICKNESS_SCALE"), std::string::npos);
	EXPECT_NE(FollowBody.find("g_Config.m_QmHudIslandBgColor"), std::string::npos);
	EXPECT_NE(FollowBody.find("g_Config.m_QmHudIslandBgOpacity"), std::string::npos);
	EXPECT_EQ(FollowBody.find("DrawMediaIslandArcGeometry"), std::string::npos);
	EXPECT_EQ(FollowBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_EQ(FollowBody.find("TextRender()"), std::string::npos);

	EXPECT_NE(SatelliteBody.find("SHudMediaIslandSdfRenderState"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("m_Radii = vec2()"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("MEDIA_ISLAND_OUTER_SHADOW_PIXELS"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("MEDIA_ISLAND_OUTER_SHADOW_OPACITY"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("QmHudMediaIslandBuildGpuSdfParams"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("RenderMediaIslandSdf"), std::string::npos);
	EXPECT_NE(SatelliteBody.find("DrawMediaIslandGeometryFallback"), std::string::npos);
	EXPECT_EQ(SatelliteBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_EQ(SatelliteBody.find("TextRender()"), std::string::npos);
}

TEST(QmHudSwitchCountdownSource, ModesShareTrackingAndCanFeedBothSurfaces)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string UpdateBody = FunctionBody(Source, "void CHud::UpdateSwitchCountdownTracker()");
	const std::string HasIslandBody = FunctionBody(Source, "bool CHud::HasActiveSwitchCountdown() const");
	const std::string FollowBody = FunctionBody(Source, "void CHud::RenderFollowSwitchCountdowns()");
	const std::string OnRenderBody = FunctionBody(Source, "void CHud::OnRender()");
	const size_t IslandBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(IslandBegin, std::string::npos);
	const size_t IslandEnd = Source.find("void CHud::RenderPlayerState", IslandBegin);
	ASSERT_NE(IslandEnd, std::string::npos);
	const std::string IslandBody = Source.substr(IslandBegin, IslandEnd - IslandBegin);

	EXPECT_NE(UpdateBody.find("m_aaClientId[Team][SwitchNumber] = ClientId"), std::string::npos);
	EXPECT_NE(UpdateBody.find("m_aaConnection[Team][SwitchNumber] = Connection"), std::string::npos);
	EXPECT_NE(UpdateBody.find("QmHudSwitchCountdownShowsFollowTee"), std::string::npos);
	EXPECT_NE(HasIslandBody.find("QmHudSwitchCountdownShowsMediaIsland"), std::string::npos);
	EXPECT_NE(IslandBody.find("g_Config.m_QmSwitchCountdown"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudSwitchCountdownShowsMediaIsland"), std::string::npos);
	EXPECT_NE(OnRenderBody.find("RenderFollowSwitchCountdowns();"), std::string::npos);
	EXPECT_NE(FollowBody.find("QmHudSwitchCountdownShowsFollowTee"), std::string::npos);
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

TEST(QmHudMediaIslandSpectatorEye, OpenGlUsesTextIconWithoutChangingTheAtlasPath)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	ASSERT_FALSE(RenderBody.empty());

	EXPECT_NE(Source.find("bool IsOpenGlBackend()"), std::string::npos);
	EXPECT_NE(Source.find("str_comp_nocase(g_Config.m_GfxBackend, \"OpenGL\") == 0"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(IsOpenGlBackend())"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE_SLASH"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE"), std::string::npos);
	EXPECT_NE(RenderBody.find("else if(CQmIconManager *pIconManager = GameClient()->QmIconManager())"), std::string::npos);
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
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(0));
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(1));
	EXPECT_TRUE(QmHudMediaIslandShouldPrepareBackdropBlur(99));
	EXPECT_FALSE(QmHudMediaIslandShouldPrepareBackdropBlur(100));
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

TEST(QmHudMediaIslandSatellite, RenderPathUsesBlobSatellitesInsteadOfCountdownText)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string GameClientSource = ReadTestSourceFile("src/game/client/gameclient.cpp");
	const std::string TClientSource = ReadTestSourceFile("src/game/client/components/tclient/tclient.cpp");
	const size_t RenderBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(RenderBegin, std::string::npos);
	const size_t RenderEnd = Source.find("void CHud::RenderPlayerState", RenderBegin);
	ASSERT_NE(RenderEnd, std::string::npos);
	const std::string RenderBody = Source.substr(RenderBegin, RenderEnd - RenderBegin);

	EXPECT_NE(RenderBody.find("RenderMediaIslandSdf"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudAdvanceMediaIslandLiquidProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobPose"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobBlend"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandBlobConnectionStrength(BlobPose.m_Travel)"), std::string::npos);
	EXPECT_NE(RenderBody.find("mix(SpawnCenterX, FinalCenterX, BlobPose.m_Travel)"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSdfOuterRect(CurrentSdfState)"), std::string::npos);
	EXPECT_NE(RenderBody.find("TransformedScreenX1 - TransformedScreenX0"), std::string::npos);
	EXPECT_NE(RenderBody.find("TransformedScreenY1 - TransformedScreenY0"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandRipple"), std::string::npos);
	EXPECT_EQ(Source.find("m_MediaIslandRippleTexture"), std::string::npos);
	EXPECT_NE(RenderBody.find("const float SatelliteRadius = Radius;"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr float SatelliteItemGap = QmHudMediaIslandScaled(2.0f);"), std::string::npos);
	EXPECT_NE(RenderBody.find("aActiveSatelliteTargetCenters[i] = SatelliteCursorX + aActiveSatelliteTargetWidths[i] * 0.5f"), std::string::npos);
	EXPECT_NE(RenderBody.find("const CUIRect MainIslandSdfRect = EntrancePose.m_Rect;"), std::string::npos);
	EXPECT_EQ(RenderBody.find("QmHudMediaIslandMainCapRect"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandRightBlobCapsule"), std::string::npos);
	EXPECT_NE(RenderBody.find("EQmIcon::SATELLITE_SPECTATOR_EYE"), std::string::npos);
	EXPECT_NE(RenderBody.find("EQmIcon::SATELLITE_SPECTATOR_EYE_CLOSED"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSpectatorCountAlpha(ShowSpectator, SpectatorIconPose)"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSpectatorIconProgressDuringExit(AnimState.m_SpectatorExitIconStart, AnimState.m_SpectatorExitLiquidStart, AnimState.m_SpectatorLiquidProgress)"), std::string::npos);
	EXPECT_NE(Source.find("EQmIcon::SATELLITE_CHECK"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(IsOpenGlBackend())"), std::string::npos);
	EXPECT_NE(RenderBody.find("FontIcons::FONT_ICON_EYE"), std::string::npos);
	EXPECT_EQ(RenderBody.find("if(SatelliteRenderItemCount > 0)"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandLiquidBridge"), std::string::npos);
	EXPECT_EQ(RenderBody.find("DrawMediaIslandProgressRing"), std::string::npos);
	EXPECT_EQ(RenderBody.find("Graphics()->DrawRect(IslandX, IslandY"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon"), std::string::npos);
	EXPECT_NE(RenderBody.find("const ColorRGBA IconColor = Item.m_Completed ? ColorRGBA(0.20f, 1.0f, 0.42f"), std::string::npos);
	EXPECT_NE(RenderBody.find("MediaIslandCountdownIcon(Item.m_Type, Item.m_Completed, Item.m_SwapOutgoing)"), std::string::npos);
	EXPECT_NE(Source.find("QmHudMediaIslandSwapVisibleForConnection(Dummy, g_Config.m_ClDummy)"), std::string::npos);
	EXPECT_NE(Source.find("Out.m_Outgoing = State.m_Outgoing"), std::string::npos);
	EXPECT_NE(Source.find("SwapOutgoing ? EQmIcon::SATELLITE_SWAP_OUTGOING : EQmIcon::SATELLITE_SWAP_INCOMING"), std::string::npos);
	EXPECT_NE(RenderBody.find("SdfItem.m_RingColor = MediaIslandCountdownColor(Item.m_Type);"), std::string::npos);
	EXPECT_EQ(RenderBody.find("MediaIslandCountdownColor(Item.m_Type, Item.m_Completed)"), std::string::npos);
	EXPECT_NE(RenderBody.find("SatelliteVisibleLeft"), std::string::npos);
	EXPECT_EQ(RenderBody.find("BuildSwitchCountdownSummary"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_Hud.HandleSpamProtectionMessage(pMsg->m_pMessage);"), std::string::npos);
	EXPECT_NE(GameClientSource.find("m_TClient.HandleSwapCountdownMessage(pMsg->m_pMessage, Conn);"), std::string::npos);
	EXPECT_NE(TClientSource.find("m_aSwapCountdownTrackers[Dummy].Cancel"), std::string::npos);
}

TEST(QmHudMediaIslandSwapText, OnlyIncomingRequestsReplaceCheckpointAndExpandBeforeLyrics)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	const std::string BuildInfoBody = FunctionBody(Source, "bool BuildSwapCountdownInfo(");

	EXPECT_NE(BuildInfoBody.find("if(!Out.m_Outgoing)"), std::string::npos);
	EXPECT_NE(BuildInfoBody.find("Localize(\"%s has requested to swap with %s\")"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandSwapRows(IncomingSwapCount, TimerCapsule.m_Visible, ShowLyricsIslandLine)"), std::string::npos);
	EXPECT_NE(RenderBody.find("TimerCapsule.m_BoxW = std::min(MaxTimerWidth"), std::string::npos);
	EXPECT_NE(RenderBody.find("if(SwapRows.m_InlineSwapCount > 0)"), std::string::npos);
	EXPECT_NE(RenderBody.find("else if(Checkpoint > 0)"), std::string::npos);
	EXPECT_NE(RenderBody.find("LyricsTextY = BottomTextY + BottomRowLineHeight * SwapRows.m_LyricsLineIndex"), std::string::npos);
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

TEST(QmHudMediaIslandTimerLayout, CheckpointOrSwapRaceTextDoesNotIntrudeIntoTopMargin)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");

	EXPECT_NE(RenderBody.find("const bool ShowTimerSecondaryLine = SwapRows.m_InlineSwapCount > 0 || Checkpoint > 0;"), std::string::npos);
	EXPECT_NE(RenderBody.find("ShowTimerSecondaryLine ? TimerRows.m_RaceY + (TimerRows.m_RaceH - TimerRaceFontSize) * 0.5f : TimerCapsule.m_TextY"), std::string::npos);
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

TEST(QmHudMediaIslandSource, MovesClockAndFrozenCountIntoStackAndReplacesClockSlotWithWaveform)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string RenderBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	const std::string VisibleBody = FunctionBody(Source, "bool CHud::HasVisibleMediaIsland() const");

	EXPECT_NE(RenderBody.find("const bool ShowInfoStack = ShowLocalTime || ShowFrozenSummary;"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr float InfoStackGap = QmHudMediaIslandScaled(0.8f);"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandMirroredInfoStack"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandWaveBarHeight"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr int WaveBarCount = 7;"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr int WaveTargetBarCount = 6;"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandWaveBarSettleProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("constexpr float WaveMaxHeight = QmHudMediaIslandScaled(7.2f);"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandTimerRows"), std::string::npos);
	EXPECT_NE(VisibleBody.find("BuildHudFrozenSummaryText"), std::string::npos);
	EXPECT_EQ(RenderBody.find("ShowFrozenSummaryInBottomRow"), std::string::npos);
	EXPECT_EQ(RenderBody.find("%s CP%d"), std::string::npos);
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
	EXPECT_NE(RenderBody.find("QmHudAdvanceMediaIslandEntranceTimeline"), std::string::npos);
	EXPECT_NE(RenderBody.find("AnimState.m_EntranceDropProgress = EntranceTimeline.m_DropProgress"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntranceDeltaSeconds, MotionLevel"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandEntrancePose"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_BackgroundColor"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_ContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("EntrancePose.m_DisabledCornerRadius"), std::string::npos);
	EXPECT_NE(RenderBody.find("CoverInAlpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("TrackTitleInAlpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("TimerCapsule.m_Alpha * EntranceContentAlpha"), std::string::npos);
	EXPECT_NE(RenderBody.find("QmHudMediaIslandDesiredBottomWidth("), std::string::npos);
	EXPECT_EQ(RenderBody.find("TextBoundingBox(BottomFontSize, aLyricsIslandBuf)"), std::string::npos);
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

TEST(QmMediaIslandGpuSdfContract, UsesFixedStd140FriendlyParameterBlock)
{
	static_assert(IGraphics::MEDIA_ISLAND_SDF_MAX_ITEMS == 12);
	static_assert(IGraphics::SMediaIslandSdfParams::DATA_RESERVED == 7);
	static_assert(IGraphics::SMediaIslandSdfParams::DATA_ITEM_STRIDE == 3);
	static_assert(IGraphics::SMediaIslandSdfParams::DATA_BACKDROP_UV == 44);
	static_assert(IGraphics::SMediaIslandSdfParams::DATA_COUNT == 45);
	static_assert(sizeof(vec4) == sizeof(float) * 4);
	static_assert(sizeof(IGraphics::SMediaIslandSdfParams) == IGraphics::SMediaIslandSdfParams::DATA_COUNT * sizeof(vec4));

	IGraphics::SMediaIslandSdfParams Params;
	Params.Clear();
	Params.SetItemCount(12);
	Params.SetHasRightCapsule(true);
	Params.SetMainCorners(IGraphics::CORNER_ALL);
	EXPECT_EQ(Params.ItemCount(), 12);
	EXPECT_TRUE(Params.HasRightCapsule());
	EXPECT_EQ(Params.MainCorners(), IGraphics::CORNER_ALL);

	Params.SetItemCount(13);
	EXPECT_EQ(Params.ItemCount(), IGraphics::MEDIA_ISLAND_SDF_MAX_ITEMS);
}

TEST(QmMediaIslandGpuSdfContract, BackendsPublishActualShaderCapability)
{
	const std::string OpenGlSource = ReadTestSourceFile("src/engine/client/backend/opengl/backend_opengl3.cpp");
	const std::string VulkanSource = ReadTestSourceFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	EXPECT_NE(OpenGlSource.find("m_MediaIslandSdf = m_MediaIslandSdfProgramValid"), std::string::npos);
	EXPECT_NE(VulkanSource.find("m_MediaIslandSdf = true"), std::string::npos);
}

TEST(QmMediaIslandGpuSdfContract, ShapePassAvoidsPerFragmentDistanceArrayAndInactiveItemIterations)
{
	const std::array<const char *, 2> apShaderPaths = {
		"data/shader/media_island_sdf.frag",
		"data/shader/vulkan/media_island_sdf.frag",
	};
	for(const char *pShaderPath : apShaderPaths)
	{
		const std::string ShaderSource = ReadTestSourceFile(pShaderPath);
		EXPECT_NE(ShaderSource.find("gMediaIslandSdfData[45]"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("ITEM_STRIDE = 3"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("BlobSdf"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("BlobExponent"), std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find("RadialRipple"), std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find("StrongestRipple"), std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find("EllipseSdf"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("float ItemDistance = BlobSdf(Point, ItemShape.xy, ItemShape.zw);"), std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find("float ItemDistances[MAX_ITEMS]"), std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find("i < MAX_ITEMS"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("float ShapeDistance = MainDistance;"), std::string::npos) << pShaderPath;
		EXPECT_NE(ShaderSource.find("vec4 ItemShape = Data(ITEM_BASE + i * ITEM_STRIDE);"), std::string::npos) << pShaderPath;

		const std::string ItemLoop = "for(int i = 0; i < ItemCount; ++i)";
		const size_t ShapeLoop = ShaderSource.find(ItemLoop);
		ASSERT_NE(ShapeLoop, std::string::npos) << pShaderPath;
		const size_t RingLoop = ShaderSource.find(ItemLoop, ShapeLoop + ItemLoop.size());
		ASSERT_NE(RingLoop, std::string::npos) << pShaderPath;
		EXPECT_EQ(ShaderSource.find(ItemLoop, RingLoop + ItemLoop.size()), std::string::npos) << pShaderPath;
	}
}

TEST(QmMediaIslandGpuSdfContract, SinglePassShapeReductionMatchesPreviousTwoPassResult)
{
	constexpr float FarDistance = 1000000.0f;
	constexpr float MainDistance = 1.75f;
	constexpr float MainRadius = 8.0f;
	const std::array<float, 12> aItemDistances = {4.0f, 1.2f, -0.5f, 8.0f, 0.0f, 3.0f, -1.0f, 2.5f, 7.0f, 0.5f, 9.0f, -0.2f};
	const std::array<float, 12> aSmoothUnions = {0.0f, 2.0f, 3.5f, 0.0f, 1.0f, 4.0f, 0.0f, 0.5f, 2.5f, 0.0f, 1.5f, 3.0f};
	const std::array<int, 3> aItemCounts = {0, 1, 12};

	for(const int ItemCount : aItemCounts)
	{
		float PreviousSatelliteDistance = FarDistance;
		for(int i = 0; i < ItemCount; ++i)
		{
			PreviousSatelliteDistance = i == 0 ? aItemDistances[i] : QmHudMediaIslandSdfSmoothUnion(PreviousSatelliteDistance, aItemDistances[i], MainRadius * 0.28f);
		}
		float PreviousShapeDistance = std::min(MainDistance, PreviousSatelliteDistance);
		for(int i = 0; i < ItemCount; ++i)
		{
			if(aSmoothUnions[i] > 0.0f)
				PreviousShapeDistance = std::min(PreviousShapeDistance, QmHudMediaIslandSdfSmoothUnion(MainDistance, aItemDistances[i], aSmoothUnions[i]));
		}

		float SinglePassSatelliteDistance = FarDistance;
		float SinglePassShapeDistance = MainDistance;
		for(int i = 0; i < ItemCount; ++i)
		{
			SinglePassSatelliteDistance = i == 0 ? aItemDistances[i] : QmHudMediaIslandSdfSmoothUnion(SinglePassSatelliteDistance, aItemDistances[i], MainRadius * 0.28f);
			if(aSmoothUnions[i] > 0.0f)
				SinglePassShapeDistance = std::min(SinglePassShapeDistance, QmHudMediaIslandSdfSmoothUnion(MainDistance, aItemDistances[i], aSmoothUnions[i]));
		}
		SinglePassShapeDistance = std::min(SinglePassShapeDistance, SinglePassSatelliteDistance);

		EXPECT_FLOAT_EQ(SinglePassShapeDistance, PreviousShapeDistance) << "item count " << ItemCount;
	}
}

TEST(QmMediaIslandGpuSdfContract, OpenGlAndVulkanUseTheSameFragmentMainPath)
{
	const std::string OpenGlSource = ReadTestSourceFile("data/shader/media_island_sdf.frag");
	const std::string VulkanSource = ReadTestSourceFile("data/shader/vulkan/media_island_sdf.frag");
	const size_t OpenGlMain = OpenGlSource.find("void main()");
	const size_t VulkanMain = VulkanSource.find("void main()");
	ASSERT_NE(OpenGlMain, std::string::npos);
	ASSERT_NE(VulkanMain, std::string::npos);

	const auto CompactWhitespace = [](std::string Source) {
		Source.erase(std::remove_if(Source.begin(), Source.end(), [](char Character) {
			return Character == ' ' || Character == '\t' || Character == '\r' || Character == '\n';
		}),
			Source.end());
		return Source;
	};
	EXPECT_EQ(CompactWhitespace(OpenGlSource.substr(OpenGlMain)), CompactWhitespace(VulkanSource.substr(VulkanMain)));
}

TEST(QmHudMediaIslandSource, MediaIslandUsesGpuSdfCommandWithoutCpuRasterization)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const size_t IslandBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(IslandBegin, std::string::npos);
	const size_t IslandEnd = Source.find("float CHud::RenderLegacyMediaInfoAt", IslandBegin);
	ASSERT_NE(IslandEnd, std::string::npos);
	const std::string IslandBody = Source.substr(IslandBegin, IslandEnd - IslandBegin);

	const size_t SdfDraw = IslandBody.find("Graphics()->RenderMediaIslandSdf");
	ASSERT_NE(SdfDraw, std::string::npos);
	EXPECT_EQ(IslandBody.find("Graphics()->RenderMediaIslandSdf", SdfDraw + 1), std::string::npos);
	EXPECT_NE(IslandBody.find("HasMediaIslandSdf"), std::string::npos);
	EXPECT_NE(Source.find("DrawMediaIslandArcGeometry"), std::string::npos);
	EXPECT_EQ(IslandBody.find("UpdateTexture"), std::string::npos);
	EXPECT_EQ(IslandBody.find("PixelX"), std::string::npos);
	EXPECT_EQ(IslandBody.find("PixelY"), std::string::npos);
	EXPECT_EQ(Source.find("m_vMediaIslandSdfPixels"), std::string::npos);
	EXPECT_EQ(Source.find("m_MediaIslandSdfTexture"), std::string::npos);
	EXPECT_EQ(IslandBody.find("BeginRenderTarget"), std::string::npos);
}

TEST(QmHudMediaIslandSource, BackgroundBlurUsesTheAnimatedCombinedSdfIncludingAtZeroOpacity)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string Header = ReadTestSourceFile("src/game/client/components/hud.h");
	const std::string GraphicsHeader = ReadTestSourceFile("src/engine/graphics.h");
	const std::string ThreadedHeader = ReadTestSourceFile("src/engine/client/graphics_threaded.h");
	const std::string ThreadedSource = ReadTestSourceFile("src/engine/client/graphics_threaded.cpp");
	const std::string OpenGlBackend = ReadTestSourceFile("src/engine/client/backend/opengl/backend_opengl3.cpp");
	const std::string VulkanBackend = ReadTestSourceFile("src/engine/client/backend/vulkan/backend_vulkan.cpp");
	const std::string PrepareBlur = FunctionBody(Source, "bool CHud::PrepareMediaIslandBlur()");
	const std::string OnRelease = FunctionBody(Source, "void CHud::OnRelease()");
	const std::string OnRender = FunctionBody(Source, "void CHud::OnRender()");
	const size_t IslandBegin = Source.find("void CHud::RenderMediaIsland()");
	ASSERT_NE(IslandBegin, std::string::npos);
	const size_t IslandEnd = Source.find("float CHud::RenderLegacyMediaInfoAt", IslandBegin);
	ASSERT_NE(IslandEnd, std::string::npos);
	const std::string IslandBody = Source.substr(IslandBegin, IslandEnd - IslandBegin);

	EXPECT_NE(Header.find("m_MediaIslandBlurSource"), std::string::npos);
	EXPECT_NE(Header.find("void OnRelease() override;"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("QmHudMediaIslandShouldPrepareBackdropBlur"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("HasMediaIslandSdf"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("IsBackbufferCaptureSupported"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("IsRenderTargetGaussianBlurSupported"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("CaptureBackbufferToRenderTarget"), std::string::npos);
	EXPECT_NE(PrepareBlur.find("GaussianBlurRenderTarget"), std::string::npos);
	EXPECT_EQ(PrepareBlur.find("m_QmBetterScoreboard"), std::string::npos);
	EXPECT_NE(OnRelease.find("DestroyMediaIslandBlurTargets"), std::string::npos);
	EXPECT_NE(OnRender.find("g_Config.m_QmHudIslandUseOriginalStyle"), std::string::npos);
	EXPECT_NE(OnRender.find("DestroyMediaIslandBlurTargets"), std::string::npos);
	EXPECT_NE(IslandBody.find("DrawMediaIslandGeometryFallback"), std::string::npos);

	EXPECT_NE(IslandBody.find("PrepareMediaIslandBlur()"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudMediaIslandBackdropUv(CurrentSdfState.m_Rect"), std::string::npos);
	EXPECT_NE(IslandBody.find("Graphics()->RenderMediaIslandSdf(GpuSdfParams, Backdrop)"), std::string::npos);
	EXPECT_EQ(Source.find("void CHud::RenderMediaIslandBlur"), std::string::npos);

	EXPECT_NE(GraphicsHeader.find("DATA_BACKDROP_UV"), std::string::npos);
	EXPECT_NE(GraphicsHeader.find("CRenderTargetHandle Backdrop"), std::string::npos);
	EXPECT_NE(ThreadedHeader.find("m_BackdropTargetId"), std::string::npos);
	EXPECT_NE(ThreadedSource.find("Cmd.m_BackdropTargetId"), std::string::npos);
	EXPECT_NE(OpenGlBackend.find("gBackdropSampler"), std::string::npos);
	EXPECT_NE(OpenGlBackend.find("Target.m_Texture"), std::string::npos);
	EXPECT_NE(VulkanBackend.find("m_StandardTexturedDescriptorSetLayout, m_QuadUniformDescriptorSetLayout"), std::string::npos);
	EXPECT_NE(VulkanBackend.find("pCommand->m_BackdropTargetId"), std::string::npos);
}

TEST(QmHudMediaIslandSource, SharedScaleCoversLayoutTimerAndEntranceWithoutMovingTheTopAnchor)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string Logic = ReadTestSourceFile("src/game/client/components/hud_media_island_logic.h");
	const std::string AvoidanceBody = FunctionBody(Source, "float CHud::GetTopIslandAvoidanceRight() const");
	const std::string IslandBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	const std::string TimerBody = FunctionBody(Source, "SHudTopTimerCapsuleInfo BuildHudTopTimerCapsuleInfo(const SHudGameTimerInfo &TimerInfo)");

	EXPECT_NE(Logic.find("QmHudMediaIslandDesignScale = 0.8f"), std::string::npos);
	EXPECT_NE(Logic.find("QmHudMediaIslandScaled(16.0f)"), std::string::npos);
	EXPECT_NE(AvoidanceBody.find("QmHudMediaIslandScaled(16.0f)"), std::string::npos);
	EXPECT_NE(AvoidanceBody.find("QmHudMediaIslandScaled(5.8f)"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudMediaIslandScaled(16.0f)"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudMediaIslandScaled(12.0f)"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudMediaIslandScaled(5.8f)"), std::string::npos);
	EXPECT_NE(IslandBody.find("const float IslandY = 0.0f;"), std::string::npos);
	EXPECT_NE(TimerBody.find("QmHudMediaIslandScaled(TimerInfo.m_FontSize)"), std::string::npos);
}

TEST(QmHudMediaIslandSource, BackdropAndOuterShadowFollowTheSameCombinedSdf)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/hud.cpp");
	const std::string OpenGlShader = ReadTestSourceFile("data/shader/media_island_sdf.frag");
	const std::string VulkanShader = ReadTestSourceFile("data/shader/vulkan/media_island_sdf.frag");
	const std::string IslandBody = FunctionBody(Source, "void CHud::RenderMediaIsland()");
	const std::string ShadowBody = FunctionBody(Source, "void DrawMediaIslandOuterShadowFallback(IGraphics *pGraphics, const SHudMediaIslandSdfRenderState &State)");
	const std::string FallbackBody = FunctionBody(Source, "void DrawMediaIslandGeometryFallback(IGraphics *pGraphics, const SHudMediaIslandSdfRenderState &State)");

	EXPECT_NE(Source.find("MEDIA_ISLAND_OUTER_SHADOW_PIXELS = 5.0f"), std::string::npos);
	EXPECT_NE(Source.find("MEDIA_ISLAND_OUTER_SHADOW_OPACITY = 0.35f"), std::string::npos);
	EXPECT_NE(IslandBody.find("m_OuterShadowSize = ScreenPixelSize * MEDIA_ISLAND_OUTER_SHADOW_PIXELS"), std::string::npos);
	EXPECT_NE(IslandBody.find("m_OuterShadowOpacity = MEDIA_ISLAND_OUTER_SHADOW_OPACITY * EntrancePose.m_BackgroundColor.a"), std::string::npos);
	EXPECT_NE(IslandBody.find("QmHudMediaIslandBackdropUv(CurrentSdfState.m_Rect"), std::string::npos);
	EXPECT_NE(ShadowBody.find("State.m_Items"), std::string::npos);
	EXPECT_NE(ShadowBody.find("State.m_HasRightCapsule"), std::string::npos);
	EXPECT_NE(FallbackBody.find("DrawMediaIslandOuterShadowFallback"), std::string::npos);

	for(const std::string *pShader : {&OpenGlShader, &VulkanShader})
	{
		const size_t ShadowParams = pShader->find("vec4 ShadowParams = Data(7);");
		const size_t ShadowComposite = pShader->find("Composite(PremulColor, Alpha, vec4(0.0, 0.0, 0.0, ShadowParams.y)");
		const size_t BackgroundComposite = pShader->find("Composite(PremulColor, Alpha, vec4(ShapeColor, 1.0), ShapeCoverage)");
		ASSERT_NE(ShadowParams, std::string::npos);
		ASSERT_NE(ShadowComposite, std::string::npos);
		ASSERT_NE(BackgroundComposite, std::string::npos);
		EXPECT_NE(pShader->find("ShapeDistance = min(ShapeDistance, SatelliteDistance);"), std::string::npos);
		EXPECT_NE(pShader->find("ShapeDistance = min(ShapeDistance, CapsuleDistance);"), std::string::npos);
		EXPECT_NE(pShader->find("texture(gBackdropSampler"), std::string::npos);
		EXPECT_NE(pShader->find("mix(Backdrop, Background.rgb, clamp(Background.a, 0.0, 1.0))"), std::string::npos);
		EXPECT_LT(ShadowParams, ShadowComposite);
		EXPECT_LT(ShadowComposite, BackgroundComposite);
	}
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
