#include "test.h"

#include <base/color.h>

#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <generated/protocol.h>

#include <game/client/components/qmclient/modes.h>
#include <game/client/components/qmclient/translate/translate_ui_settings.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

static void ExpectColorNear(const ColorRGBA &Color, const ColorRGBA &Expected)
{
	EXPECT_NEAR(Color.r, Expected.r, 0.02f);
	EXPECT_NEAR(Color.g, Expected.g, 0.02f);
	EXPECT_NEAR(Color.b, Expected.b, 0.02f);
	EXPECT_NEAR(Color.a, Expected.a, 0.02f);
}

TEST(QmLocalSkinSource, DdnetAndAxiomKeepTeeMenuOverride)
{
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("DDRaceNetwork", "", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("ddnet", "", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("", "DDNet", "", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "axiom-cn", ""));
	EXPECT_FALSE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "", "Axiom"));
}

TEST(QmLocalSkinSource, OtherServersUseServerControlledSkin)
{
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("InfClass", "InfClass", "", ""));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("MMO", "MMO", "", ""));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin("Gores", "Gores", "kog", "KoG"));
	EXPECT_TRUE(ShouldUseServerControlledLocalSkin(nullptr, nullptr, nullptr, nullptr));
}

TEST(QmServerSkinProtocol, SixupConnectionWithoutServerPartsUsesNoSkinDisposition)
{
	// 0.7 连接：服务器白名单要求使用服务器皮肤，但本地客户端尚未收到部件下发。
	EXPECT_EQ(ResolveServerSkinProtocol(true, true, false), EServerSkinProtocol::NONE);
}

TEST(QmServerSkinProtocol, SixupConnectionWithServerPartsUsesSevenParts)
{
	EXPECT_EQ(ResolveServerSkinProtocol(true, true, true), EServerSkinProtocol::SEVEN);
}

TEST(QmServerSkinProtocol, SixupConnectionIgnoringServerPartsUsesNoSkinDisposition)
{
	// 0.7 连接但服务器在白名单内（例如 axiom），不使用服务器端皮肤部件。
	EXPECT_EQ(ResolveServerSkinProtocol(true, false, true), EServerSkinProtocol::NONE);
}

TEST(QmServerSkinProtocol, SixConnectionAlwaysUsesSixPartsRegardlessOfServerWhitelist)
{
	// 回归防护：0.6 连接即使服务器不在白名单内，也只能使用六部位皮肤。
	// 此前该组合会错误地走到 0.7 皮肤路径，把 0.6 服务器渲染成 0.7 默认皮肤。
	EXPECT_EQ(ResolveServerSkinProtocol(false, true, false), EServerSkinProtocol::SIX);
	EXPECT_EQ(ResolveServerSkinProtocol(false, true, true), EServerSkinProtocol::SIX);
	EXPECT_EQ(ResolveServerSkinProtocol(false, false, false), EServerSkinProtocol::SIX);
	EXPECT_EQ(ResolveServerSkinProtocol(false, false, true), EServerSkinProtocol::SIX);
}

TEST(QmStatisticsModeDisplay, UsesAxiomCompletedMapsAndPlaytimeForAxiomGores)
{
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(0, 1054, true, true, 347, 7200, false, -1);
	EXPECT_EQ(Display.m_Maps, 347);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 7200);
}

TEST(QmStatisticsModeDisplay, ShowsAxiomGoresWhenRemoteResultExistsWithoutLocalHistory)
{
	EXPECT_TRUE(QmStatisticsShouldShowAxiomGores(false, false, true));
	EXPECT_TRUE(QmStatisticsShouldShowAxiomGores(true, false, false));
	EXPECT_TRUE(QmStatisticsShouldShowAxiomGores(false, true, false));
	EXPECT_FALSE(QmStatisticsShouldShowAxiomGores(false, false, false));
}

TEST(QmStatisticsModeDisplay, KeepsLocalValuesWhileAxiomDataIsLoading)
{
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(3, 1054, true, false, 347, 7200, false, -1);
	EXPECT_EQ(Display.m_Maps, 3);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 1054);
}

TEST(QmStatisticsModeDisplay, UsesDdnetFinishesWithoutReplacingLocalPlaytime)
{
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(0, 701, false, false, 0, 0, true, 2154);
	EXPECT_EQ(Display.m_Maps, 2154);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 701);
}

TEST(QmStatisticsModeDisplay, UsesOfficialDdnetPlaytimeWhenAvailable)
{
	// 官方 DDNet 统计给出 2848 小时生涯时长时，该行应显示官方时长而非本机记录的 701 秒。
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(0, 701, false, false, 0, 0, true, 2154, 2848);
	EXPECT_EQ(Display.m_Maps, 2154);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 2848 * 3600);
}

TEST(QmStatisticsModeDisplay, FallsBackToLocalPlaytimeWhenDdnetHoursMissing)
{
	// 官方时长缺失（-1）时不能把本地时长清零或改成 0。
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(0, 701, false, false, 0, 0, true, 2154, -1);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 701);
}

TEST(QmStatisticsModeDisplay, DoesNotApplyDdnetHoursToNonDdnetMode)
{
	// 时序数据只属于 DDNet 行，不能泄露到 Axiom/其他模式的时长上。
	const SQmStatisticsModeDisplay Display = ResolveQmStatisticsModeDisplay(9, 123, false, false, 0, 0, false, -1, 2848);
	EXPECT_EQ(Display.m_Maps, 9);
	EXPECT_EQ(Display.m_PlaytimeSeconds, 123);
}

namespace
{
	struct SQmTestModeStats
	{
		std::string m_GameMode;
		std::string m_CommunityId;
		bool m_IsAxiom = false;
		int m_Maps = 0;
		int64_t m_Score = 0;
		int64_t m_PlaytimeSeconds = 0;
	};
} // namespace

TEST(QmStatisticsModeCollapse, FoldsDuplicateDDraceVariantsIntoSingleEntry)
{
	// 回归：本地按服务器社区分条的 DDrace 记录加上 DDStats 追加的同名条目，
	// 会让统计页渲染出多条完全相同的「DDraceNetwork · DDNet」图例与饼图切片。
	std::vector<SQmTestModeStats> vStats;
	vStats.push_back({"DDraceNetwork", "ddnet", false, 2000, 15000, 100 * 3600});
	vStats.push_back({"DDraceNetwork", "ddstats", false, 100, 1000, 20 * 3600});
	vStats.push_back({"DDNet", "", false, 56, 719, 5 * 3600});
	vStats.push_back({"Gores", "axiom", true, 12, 600, 3 * 3600});

	const auto IsDDrace = [](const SQmTestModeStats &Stats) {
		return Stats.m_GameMode == "DDraceNetwork" || Stats.m_GameMode == "DDNet";
	};

	EXPECT_TRUE(QmCollapseModeEntries(vStats, IsDDrace));
	ASSERT_EQ(vStats.size(), 2u);
	EXPECT_EQ(vStats[0].m_GameMode, "DDraceNetwork");
	EXPECT_EQ(vStats[0].m_CommunityId, "ddnet");
	EXPECT_EQ(vStats[0].m_Maps, 2000 + 100 + 56);
	EXPECT_EQ(vStats[0].m_Score, 15000 + 1000 + 719);
	EXPECT_EQ(vStats[0].m_PlaytimeSeconds, (100 + 20 + 5) * 3600);
	// 未命中的条目保持原样。
	EXPECT_EQ(vStats[1].m_GameMode, "Gores");
	EXPECT_EQ(vStats[1].m_CommunityId, "axiom");
	EXPECT_EQ(vStats[1].m_Maps, 12);
	EXPECT_EQ(vStats[1].m_PlaytimeSeconds, 3 * 3600);

	// 只剩一条 DDrace 时再次折叠不应有变化。
	EXPECT_FALSE(QmCollapseModeEntries(vStats, IsDDrace));
	EXPECT_EQ(vStats.size(), 2u);
}

TEST(QmStatisticsModeCollapse, SaturatesFoldedValuesAtTypeLimits)
{
	std::vector<SQmTestModeStats> vStats;
	vStats.push_back({"DDraceNetwork", "ddnet", false, std::numeric_limits<int>::max(), 0, 0});
	vStats.push_back({"DDraceNetwork", "ddstats", false, 5, 0, 0});
	EXPECT_TRUE(QmCollapseModeEntries(vStats, [](const SQmTestModeStats &Stats) { return Stats.m_GameMode == "DDraceNetwork"; }));
	ASSERT_EQ(vStats.size(), 1u);
	EXPECT_EQ(vStats[0].m_Maps, std::numeric_limits<int>::max());
}

TEST(QmStatisticsChart, FallsBackToPlaytimeWhenNoModeHasFinishedMaps)
{
	EXPECT_EQ(QmStatisticsChartWeight(0, 1054, false), 1054);
	EXPECT_EQ(QmStatisticsChartWeight(0, 701, false), 701);
	EXPECT_EQ(QmStatisticsChartWeight(347, 7200, true), 347);
}

TEST(LocalSkinSource, DemoPlaybackUsesRecordedSnapshotForEitherLocalConnection)
{
	constexpr int MainClientId = 7;
	constexpr int DummyClientId = 19;

	EXPECT_EQ(ResolveLocalSkinConfigIndex(true, MainClientId, MainClientId, DummyClientId), -1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(true, DummyClientId, MainClientId, DummyClientId), -1);
}

TEST(LocalSkinSource, OnlinePlayUsesMatchingLocalConfiguration)
{
	constexpr int MainClientId = 7;
	constexpr int DummyClientId = 19;

	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, MainClientId, MainClientId, DummyClientId), 0);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, DummyClientId, MainClientId, DummyClientId), 1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, 23, MainClientId, DummyClientId), -1);
	EXPECT_EQ(ResolveLocalSkinConfigIndex(false, -1, -1, -1), -1);
}

TEST(QmFastInputMode, NormalizesLegacyModesToBestInput)
{
	EXPECT_EQ(QmFastInputNormalizedMode(0), 0);
	EXPECT_EQ(QmFastInputNormalizedMode(1), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(2), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(3), 3);
	EXPECT_EQ(QmFastInputNormalizedMode(4), 4);
}

TEST(QmFastInputMode, ComputesFastBestAndSaikoOffsets)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.0f);

	Settings.m_Mode = 3;
	Settings.m_BestOffset = 250;
	Settings.m_BestSmoothing = 50;
	Settings.m_BestLatencyComp = 20;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 2.25f);

	Settings.m_Mode = 4;
	Settings.m_SaikoPlusAmount = 175;
	EXPECT_FLOAT_EQ(QmEffectiveFastInputOffsetTicks(Settings), 1.75f);
}

TEST(QmFastInputMode, PredictionTicksUseSaikoPlusExtraLocalTickOnly)
{
	EXPECT_EQ(QmFastInputPredictionTicks(0.01f, 0), 1);
	EXPECT_EQ(QmFastInputPredictionTicks(1.25f, 3), 2);
	EXPECT_EQ(QmFastInputPredictionTicks(1.25f, 4), 3);
	EXPECT_EQ(QmFastInputPredictionTicksOthers(1.25f, 4), 2);
}

TEST(QmFastInputMode, AppliesOffsetWithoutNegativeIntra)
{
	int Tick = 100;
	float Intra = 0.20f;
	QmApplyFastInputOffset(1.25f, Tick, Intra);
	EXPECT_EQ(Tick, 101);
	EXPECT_FLOAT_EQ(Intra, 0.45f);
}

TEST(QmFastInputMode, ChoosesOthersToggleByMode)
{
	EXPECT_FALSE(QmEffectiveFastInputOthers(false, 0, true, true, true));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 0, true, false, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 3, false, true, false));
	EXPECT_TRUE(QmEffectiveFastInputOthers(true, 4, false, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 3, true, false, true));
	EXPECT_FALSE(QmEffectiveFastInputOthers(true, 4, true, true, false));
}

TEST(QmFastInputMode, MarginUsesLargestFastInputContribution)
{
	SQmFastInputSettings Settings;
	Settings.m_Enabled = true;
	Settings.m_BasePredictionMarginMs = 10;

	Settings.m_Mode = 0;
	Settings.m_FastAmountMs = 40;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 40);

	Settings.m_Mode = 3;
	Settings.m_BestOffset = 250;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 50);

	Settings.m_Mode = 4;
	Settings.m_SaikoPlusAmount = 175;
	EXPECT_EQ(QmFastInputBasePredictionMarginMs(Settings), 35);
}

TEST(QmFastInputMode, AutoPredictionMarginKeepsStableBase)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 0.0f, false), 10);
}

TEST(QmFastInputMode, AutoPredictionMarginAddsLatencyJitterAndConnectionProtection)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 70.0f, 10.0f, 10.0f, 0.0f, false), 20);
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 14.0f, false), 19);
	EXPECT_EQ(QmComputeAutoPredictionMargin(10, 0.0f, 10.0f, 10.0f, 0.0f, true), 20);
}

TEST(QmFastInputMode, AutoPredictionMarginClampsToSupportedRange)
{
	EXPECT_EQ(QmComputeAutoPredictionMargin(0, 0.0f, 0.0f, 0.0f, 0.0f, false), 1);
	EXPECT_EQ(QmComputeAutoPredictionMargin(500, 0.0f, 0.0f, 0.0f, 0.0f, false), 300);
}

TEST(QmNameplateHookStrongWeak, ScopeFiltersExpectedPlayers)
{
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_SELF, true, false, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_SELF, false, true, false));

	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, true, false, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, false, true, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_OTHERS, false, false, true));

	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_STRONG, false, true, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_STRONG, false, false, true));

	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_WEAK, false, true, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_WEAK, false, false, true));

	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_ALL, true, false, false));
	EXPECT_TRUE(ShouldShowQmHookStrongWeakScope(QM_HOOK_STRONG_WEAK_SCOPE_ALL, false, true, false));
	EXPECT_FALSE(ShouldShowQmHookStrongWeakScope(99, false, true, false));
}

TEST(QmNameplateTextEffects, PlayingScopeSupportsSelfOthersFriendsAndAll)
{
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 1));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 2));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 3));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, true, false, false, 0));
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, true, false, 3));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_SELF_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 4));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, false, false, false, false, 4));
}

TEST(QmNameplateTextEffects, SpectateScopeDoesNotUsePlayingScope)
{
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, false, 8));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OTHERS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, true, false, 8));
	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_TARGET_FRIENDS, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, -1, false, true, false, false, true, 7));
}

TEST(QmNameplateTextEffects, DemoModesOverridePlayingAndSpectateScopes)
{
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_OFF, 5, true, true, true, true, true, 5));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_SMART, -1, true, true, false, false, true, 5));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_SMART, -1, true, true, false, false, false, 6));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_OFF, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET, 5, true, true, false, false, false, 5));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_ALL, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_TARGET, 5, true, true, true, true, true, 6));

	EXPECT_TRUE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_OFF, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE, -1, true, true, false, true, false, 6));
	EXPECT_FALSE(ShouldUseQmNameplateTextEffects(QM_NAMEPLATE_TEXT_PLAYING_SCOPE_FRIENDS, QM_NAMEPLATE_TEXT_SPECTATE_SCOPE_ALL, QM_NAMEPLATE_TEXT_DEMO_MODE_MANUAL_SCOPE, -1, true, true, false, false, true, 5));
}

TEST(QmFocusMode, ConfigOverrideRestoresOnlyAutoHiddenValues)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	int Value = ApplyQmFocusConfigOverride(State, true, 1, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 0);
	EXPECT_TRUE(State.m_WasActive);
	EXPECT_EQ(State.m_SavedValue, 1);

	Value = ApplyQmFocusConfigOverride(State, false, 0, 0, Changed);
	EXPECT_TRUE(Changed);
	EXPECT_EQ(Value, 1);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, ConfigOverrideKeepsUserChangesMadeWhileActive)
{
	SQmFocusConfigOverrideState State;
	bool Changed = false;

	EXPECT_EQ(ApplyQmFocusConfigOverride(State, true, 1, 0, Changed), 0);
	EXPECT_TRUE(Changed);

	const int UserChangedValue = 2;
	EXPECT_EQ(ApplyQmFocusConfigOverride(State, false, UserChangedValue, 0, Changed), UserChangedValue);
	EXPECT_FALSE(Changed);
	EXPECT_FALSE(State.m_WasActive);
}

TEST(QmFocusMode, EveryChildToggleRequiresMasterSwitchAndItsOwnFlag)
{
	// 禅模式每个子开关只有在"总开关开启 且 该子开关开启"时才生效；
	// 这张矩阵同时覆盖总开关关闭 + 子开关开启这类此前容易漏掉的组合。
	struct SCase
	{
		bool SQmFocusModeConfig::*m_pSetting;
		bool SQmFocusModeDecisions::*m_pDecision;
	};
	const SCase aCases[] = {
		{&SQmFocusModeConfig::m_HideHud, &SQmFocusModeDecisions::m_HideHud},
		{&SQmFocusModeConfig::m_HideMapProgress, &SQmFocusModeDecisions::m_HideMapProgress},
		{&SQmFocusModeConfig::m_HideScoreboard, &SQmFocusModeDecisions::m_HideScoreboard},
		{&SQmFocusModeConfig::m_HideNames, &SQmFocusModeDecisions::m_HideNames},
		{&SQmFocusModeConfig::m_HideNameplates, &SQmFocusModeDecisions::m_HideNameplates},
		{&SQmFocusModeConfig::m_HideInfoMessages, &SQmFocusModeDecisions::m_HideInfoMessages},
		{&SQmFocusModeConfig::m_HideDirectionIndicators, &SQmFocusModeDecisions::m_HideDirectionIndicators},
		{&SQmFocusModeConfig::m_HideGuideLines, &SQmFocusModeDecisions::m_HideGuideLines},
		{&SQmFocusModeConfig::m_HideKillEffects, &SQmFocusModeDecisions::m_HideKillEffects},
		{&SQmFocusModeConfig::m_HideExplosionEffects, &SQmFocusModeDecisions::m_HideExplosionEffects},
		{&SQmFocusModeConfig::m_HideFreezeEffects, &SQmFocusModeDecisions::m_HideFreezeEffects},
		{&SQmFocusModeConfig::m_HideHammerEffects, &SQmFocusModeDecisions::m_HideHammerEffects},
		{&SQmFocusModeConfig::m_HideMuzzleEffects, &SQmFocusModeDecisions::m_HideMuzzleEffects},
		{&SQmFocusModeConfig::m_MuteDeathSounds, &SQmFocusModeDecisions::m_MuteDeathSounds},
		{&SQmFocusModeConfig::m_MuteHammerSounds, &SQmFocusModeDecisions::m_MuteHammerSounds},
		{&SQmFocusModeConfig::m_HidePlayerMessages, &SQmFocusModeDecisions::m_HidePlayerMessages},
		{&SQmFocusModeConfig::m_HideSystemInfoMessages, &SQmFocusModeDecisions::m_HideSystemInfoMessages},
		{&SQmFocusModeConfig::m_HideSystemPromptMessages, &SQmFocusModeDecisions::m_HideSystemPromptMessages},
		{&SQmFocusModeConfig::m_HideEchoMessages, &SQmFocusModeDecisions::m_HideEchoMessages},
	};
	for(const SCase &Case : aCases)
	{
		SQmFocusModeConfig Config;
		Config.*Case.m_pSetting = true;
		EXPECT_FALSE(GetQmFocusModeDecisions(Config).*Case.m_pDecision);

		Config.m_FocusActive = true;
		EXPECT_TRUE(GetQmFocusModeDecisions(Config).*Case.m_pDecision);
	}

	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	for(const SCase &Case : aCases)
		EXPECT_FALSE(Decisions.*Case.m_pDecision);
}

TEST(QmFocusMode, SpectatorHudStaysVisibleWhenFocusModeAutoHidesMainHud)
{
	EXPECT_TRUE(ShouldRenderFocusSpectatorHud(true, true, false, true));
	EXPECT_TRUE(ShouldRenderFocusSpectatorHud(true, true, true, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(false, true, false, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, false, false, true));
	EXPECT_FALSE(ShouldRenderFocusSpectatorHud(true, true, false, false));
}

TEST(QmFocusMode, SettingsSnapshotMapsEachSettingToItsOwnDecision)
{
	// 每个 qm_focus_mode_* 配置项都必须映射到唯一对应的决策字段。逐项单独打开，
	// 并校验其余字段保持关闭，可以暴露 HideChat 与 HideSystemMessages 之类的错位映射。
	// 跳跃特效/跳跃音效被折叠进 m_AirJump，单独在下面的用例覆盖。
	struct SCase
	{
		const char *m_pName;
		int CConfig::*m_pSetting;
		bool SQmFocusModeDecisions::*m_pDecision;
	};
	const SCase aCases[] = {
		{"qm_focus_mode_hide_hud", &CConfig::m_QmFocusModeHideHud, &SQmFocusModeDecisions::m_HideHud},
		{"qm_focus_mode_hide_map_progress", &CConfig::m_QmFocusModeHideMapProgress, &SQmFocusModeDecisions::m_HideMapProgress},
		{"qm_focus_mode_hide_info_messages", &CConfig::m_QmFocusModeHideInfoMessages, &SQmFocusModeDecisions::m_HideInfoMessages},
		{"qm_focus_mode_hide_scoreboard", &CConfig::m_QmFocusModeHideScoreboard, &SQmFocusModeDecisions::m_HideScoreboard},
		{"qm_focus_mode_hide_names", &CConfig::m_QmFocusModeHideNames, &SQmFocusModeDecisions::m_HideNames},
		{"qm_focus_mode_hide_nameplates", &CConfig::m_QmFocusModeHideNameplates, &SQmFocusModeDecisions::m_HideNameplates},
		{"qm_focus_mode_hide_direction_indicators", &CConfig::m_QmFocusModeHideDirectionIndicators, &SQmFocusModeDecisions::m_HideDirectionIndicators},
		{"qm_focus_mode_hide_guide_lines", &CConfig::m_QmFocusModeHideGuideLines, &SQmFocusModeDecisions::m_HideGuideLines},
		{"qm_focus_mode_hide_kill_effects", &CConfig::m_QmFocusModeHideKillEffects, &SQmFocusModeDecisions::m_HideKillEffects},
		{"qm_focus_mode_hide_explosion_effects", &CConfig::m_QmFocusModeHideExplosionEffects, &SQmFocusModeDecisions::m_HideExplosionEffects},
		{"qm_focus_mode_hide_freeze_effects", &CConfig::m_QmFocusModeHideFreezeEffects, &SQmFocusModeDecisions::m_HideFreezeEffects},
		{"qm_focus_mode_hide_hammer_effects", &CConfig::m_QmFocusModeHideHammerEffects, &SQmFocusModeDecisions::m_HideHammerEffects},
		{"qm_focus_mode_hide_muzzle_effects", &CConfig::m_QmFocusModeHideMuzzleEffects, &SQmFocusModeDecisions::m_HideMuzzleEffects},
		{"qm_focus_mode_mute_death_sounds", &CConfig::m_QmFocusModeMuteDeathSounds, &SQmFocusModeDecisions::m_MuteDeathSounds},
		{"qm_focus_mode_mute_hammer_sounds", &CConfig::m_QmFocusModeMuteHammerSounds, &SQmFocusModeDecisions::m_MuteHammerSounds},
		{"qm_focus_mode_hide_chat", &CConfig::m_QmFocusModeHideChat, &SQmFocusModeDecisions::m_HidePlayerMessages},
		{"qm_focus_mode_hide_system_info_messages", &CConfig::m_QmFocusModeHideSystemInfoMessages, &SQmFocusModeDecisions::m_HideSystemInfoMessages},
		{"qm_focus_mode_hide_system_messages", &CConfig::m_QmFocusModeHideSystemMessages, &SQmFocusModeDecisions::m_HideSystemPromptMessages},
		{"qm_focus_mode_hide_echo", &CConfig::m_QmFocusModeHideEcho, &SQmFocusModeDecisions::m_HideEchoMessages},
	};
	struct SConfigRestore
	{
		CConfig m_Config = g_Config;
		~SConfigRestore() { g_Config = m_Config; }
	} ConfigRestore;

	for(const SCase &Case : aCases)
		g_Config.*Case.m_pSetting = 0;
	g_Config.m_QmFocusMode = 1;

	for(const SCase &Case : aCases)
	{
		g_Config.*Case.m_pSetting = 1;
		const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions();
		for(const SCase &Other : aCases)
			EXPECT_EQ(Decisions.*Other.m_pDecision, &Case == &Other) << Case.m_pName << " 与 " << Other.m_pName << " 的映射错位";
		g_Config.*Case.m_pSetting = 0;
	}
}

TEST(QmFocusMode, IndependentMapProgressUsesItsOwnToggleAndBottomStyle)
{
	EXPECT_FALSE(ShouldRenderMapProgressBar(false, 0, false, true));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 1, false, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 1, true, true));
	EXPECT_FALSE(ShouldRenderMapProgressBar(true, 0, false, false));
	EXPECT_TRUE(ShouldRenderMapProgressBar(true, 0, false, true));
}

TEST(QmFocusMode, JumpEffectsAndJumpSoundAreDecidedIndependently)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_SoundEnabled = true;
	Config.m_MuteJumpSounds = true;
	SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_FALSE(Decisions.m_AirJump.m_PlaySound);

	Config.m_MuteJumpSounds = false;
	Config.m_HideJumpEffects = true;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);

	Config.m_FocusActive = false;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
}

TEST(QmFocusMode, SoundEnabledGatesOnlyAudibleDecisions)
{
	// 关闭游戏音效时不能播放禅模式本来就该静音的声音，但静音判定本身仍然成立。
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_MuteJumpSounds = true;
	Config.m_MuteDeathSounds = true;
	Config.m_MuteHammerSounds = true;
	Config.m_SoundEnabled = false;

	SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_AirJump.m_PlaySound);
	EXPECT_FALSE(Decisions.m_PlayDeathOrSpawnSound);
	EXPECT_TRUE(Decisions.m_MuteDeathSounds);
	EXPECT_TRUE(Decisions.m_MuteHammerSounds);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);

	Config.m_MuteJumpSounds = false;
	Config.m_MuteDeathSounds = false;
	Config.m_SoundEnabled = true;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
	EXPECT_TRUE(Decisions.m_PlayDeathOrSpawnSound);
}

TEST(QmFocusMode, ForceVisibleClientLinesRemainVisibleWhenChatIsHidden)
{
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, true, true, -2, true, false));
	EXPECT_FALSE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, true, true));
}

TEST(QmFocusMode, ChatFiltersSeparatePlayerSystemAndEchoMessages)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(true, false, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, false, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, true, false, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -1, false, true));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -2, false, false));

	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -2, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, 3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(false, false, false, true, -1, false, false));
}

TEST(QmFocusMode, UnknownChatLinesFollowSystemMessageVisibility)
{
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(false, false, true, false, -3, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(true, true, false, true, -3, false, false));
}

TEST(QmFocusMode, ChatAreaRendersWhenAnyMessageClassIsVisible)
{
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(false, true, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, false, true, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, false, true, false));
	EXPECT_TRUE(ShouldRenderAnyFocusFilteredChat(true, true, true, false, false));
}

TEST(QmFocusMode, ConfigSnapshotKeepsExplicitVisualChildrenIndependent)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
	EXPECT_FALSE(Decisions.m_HideKillEffects);
	EXPECT_FALSE(Decisions.m_HideExplosionEffects);
	EXPECT_FALSE(Decisions.m_HideFreezeEffects);
	EXPECT_FALSE(Decisions.m_HideHammerEffects);
	EXPECT_FALSE(Decisions.m_HideMuzzleEffects);

	Config.m_HideMuzzleEffects = true;
	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_HideMuzzleEffects);
}

TEST(QmFocusMode, AllEffectsSuppressedWhileRecordingVideo)
{
	// "禅模式的一切效果都不进视频"：录制期间决策按总开关关闭处理，实时画面不受影响。
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HideHud = true;
	Config.m_HideNameplates = true;
	Config.m_HidePlayerMessages = true;
	Config.m_MuteHammerSounds = true;
	Config.m_HideMuzzleEffects = true;

	EXPECT_TRUE(GetQmFocusModeDecisions(Config).m_HideHud);

	Config.m_VideoRecording = true;
	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_FocusActive);
	EXPECT_FALSE(Decisions.m_HideHud);
	EXPECT_FALSE(Decisions.m_HideNameplates);
	EXPECT_FALSE(Decisions.m_HidePlayerMessages);
	EXPECT_FALSE(Decisions.m_MuteHammerSounds);
	EXPECT_FALSE(Decisions.m_HideMuzzleEffects);
	EXPECT_TRUE(Decisions.m_AirJump.m_SpawnParticles);
	EXPECT_TRUE(Decisions.m_AirJump.m_PlaySound);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesNameTextFromWholeNameplate)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HideNames = true;

	SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_FALSE(Decisions.m_HideNameplates);

	Config.m_HideNames = false;
	Config.m_HideNameplates = true;
	Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_FALSE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
}

TEST(QmFocusMode, ConfigSnapshotSeparatesChatMessageClasses)
{
	SQmFocusModeConfig Config;
	Config.m_FocusActive = true;
	Config.m_HidePlayerMessages = true;
	Config.m_HideSystemInfoMessages = false;
	Config.m_HideSystemPromptMessages = true;
	Config.m_HideEchoMessages = true;
	Config.m_HideHud = true;
	Config.m_HideScoreboard = true;
	Config.m_HideNames = true;
	Config.m_HideNameplates = true;

	const SQmFocusModeDecisions Decisions = GetQmFocusModeDecisions(Config);
	EXPECT_TRUE(Decisions.m_HideHud);
	EXPECT_TRUE(Decisions.m_HideScoreboard);
	EXPECT_TRUE(Decisions.m_HideNames);
	EXPECT_TRUE(Decisions.m_HideNameplates);
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, 0, false, false));
	EXPECT_TRUE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, true));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -1, false, false));
	EXPECT_FALSE(ShouldRenderFocusFilteredChatLine(Decisions.m_HidePlayerMessages, Decisions.m_HideSystemInfoMessages, Decisions.m_HideSystemPromptMessages, Decisions.m_HideEchoMessages, -2, false, false));
}

TEST(QmTranslateUiSettings, DefaultColorsMatchSettingsPreviewDefaults)
{
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateBtnColorDisabled, true)), ColorRGBA(0.16f, 0.16f, 0.16f, 0.82f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateBtnColorEnabled, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuBgColor, true)), ColorRGBA(0.12f, 0.12f, 0.12f, 0.95f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuOptionSelected, true)), ColorRGBA(0.35f, 0.45f, 0.70f, 0.90f));
	ExpectColorNear(color_cast<ColorRGBA>(ColorHSLA(DefaultConfig::QmTranslateMenuOptionNormal, true)), ColorRGBA(0.20f, 0.20f, 0.20f, 0.90f));
}

TEST(QmTranslateUiSettings, LegacyRgbColorsRestoreDeclaredAlpha)
{
	bool Migrated = false;
	unsigned Disabled = 0x005A6B7Cu;
	unsigned Enabled = 0x00010203u;
	unsigned Background = 0x00A1B2C3u;
	unsigned Selected = 0x00000000u;
	unsigned Normal = 0x00D4E5F6u;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED));
	EXPECT_TRUE(Migrated);
	EXPECT_EQ(Disabled, 0xD15A6B7Cu);
	EXPECT_EQ(Enabled, 0xE6010203u);
	EXPECT_EQ(Background, 0xF2A1B2C3u);
	EXPECT_EQ(Selected, 0xE6000000u);
	EXPECT_EQ(Normal, 0xE6D4E5F6u);
}

TEST(QmTranslateUiSettings, AlphaAwareColorsAreNotChanged)
{
	bool Migrated = false;
	unsigned Disabled = 0x7F5A6B7Cu;
	unsigned Enabled = 0x805A6B7Cu;
	unsigned Background = 0x995A6B7Cu;
	unsigned Selected = 0xA05A6B7Cu;
	unsigned Normal = 0xB15A6B7Cu;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT));
	EXPECT_EQ(Disabled, 0x7F5A6B7Cu);
	EXPECT_EQ(Enabled, 0x805A6B7Cu);
	EXPECT_EQ(Background, 0x995A6B7Cu);
	EXPECT_EQ(Selected, 0xA05A6B7Cu);
	EXPECT_EQ(Normal, 0xB15A6B7Cu);
}

TEST(QmTranslateUiSettings, PackedColorsWithNonZeroAlphaAreNotChanged)
{
	bool Migrated = false;
	unsigned Disabled = 0x7F5A6B7Cu;
	unsigned Enabled = 0x805A6B7Cu;
	unsigned Background = 0x995A6B7Cu;
	unsigned Selected = 0xA05A6B7Cu;
	unsigned Normal = 0xB15A6B7Cu;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED, EColorInputAlphaMode::PACKED));
	EXPECT_EQ(Disabled, 0x7F5A6B7Cu);
	EXPECT_EQ(Enabled, 0x805A6B7Cu);
	EXPECT_EQ(Background, 0x995A6B7Cu);
	EXPECT_EQ(Selected, 0xA05A6B7Cu);
	EXPECT_EQ(Normal, 0xB15A6B7Cu);
}

TEST(QmTranslateUiSettings, ImplicitAlphaInputsRestoreDeclaredAlpha)
{
	bool Migrated = false;
	unsigned Disabled = 0xFF5A6B7Cu;
	unsigned Enabled = 0xFF010203u;
	unsigned Background = 0xFFA1B2C3u;
	unsigned Selected = 0xFF000000u;
	unsigned Normal = 0xFFD4E5F6u;
	EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
		EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED, EColorInputAlphaMode::OMITTED));
	EXPECT_EQ(Disabled, 0xD15A6B7Cu);
	EXPECT_EQ(Enabled, 0xE6010203u);
	EXPECT_EQ(Background, 0xF2A1B2C3u);
	EXPECT_EQ(Selected, 0xE6000000u);
	EXPECT_EQ(Normal, 0xE6D4E5F6u);
}

TEST(QmTranslateUiSettings, ConfigManagerRecordsColorAlphaInputModes)
{
	struct SConfigRestore
	{
		CConfig m_Config = g_Config;
		~SConfigRestore() { g_Config = m_Config; }
	} ConfigRestore;
	CTestInfo TestInfo;
	std::unique_ptr<IStorage> pStorage = TestInfo.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);
	std::unique_ptr<IKernel> pKernel(IKernel::Create());
	pKernel->RegisterInterface(pStorage.get(), false);
	IConsole *pConsole = CreateConsole(CFGFLAG_CLIENT).release();
	pKernel->RegisterInterface(pConsole);
	IConfigManager *pConfigManager = CreateConfigManager();
	pKernel->RegisterInterface(pConfigManager);
	pConsole->Init();
	pConfigManager->Init();

	const auto MigrateDisabledColor = [pConfigManager]() {
		bool Migrated = false;
		unsigned Disabled = g_Config.m_QmTranslateBtnColorDisabled;
		unsigned Enabled = DefaultConfig::QmTranslateBtnColorEnabled;
		unsigned Background = DefaultConfig::QmTranslateMenuBgColor;
		unsigned Selected = DefaultConfig::QmTranslateMenuOptionSelected;
		unsigned Normal = DefaultConfig::QmTranslateMenuOptionNormal;
		EXPECT_TRUE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
			DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
			DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal,
			pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT, EColorInputAlphaMode::EXPLICIT));
		return Disabled;
	};

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned OmittedRgb = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (OmittedRgb & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $ABC");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned OmittedShortRgb = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (OmittedShortRgb & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C7F");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), ExplicitAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $ABCD");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitShortAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), ExplicitShortAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled $5A6B7C00");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::EXPLICIT);
	const unsigned ExplicitTransparent = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), ExplicitTransparent);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled red");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::OMITTED);
	const unsigned NamedColor = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_EQ(MigrateDisabledColor(), (NamedColor & ~NTranslateUiSettings::COLOR_ALPHA_MASK) | (DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK));

	pConsole->ExecuteLine("qm_translate_btn_color_disabled -16777216");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::SIGNED_PACKED);
	EXPECT_EQ(MigrateDisabledColor() & NTranslateUiSettings::COLOR_ALPHA_MASK, DefaultConfig::QmTranslateBtnColorDisabled & NTranslateUiSettings::COLOR_ALPHA_MASK);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled 2153407356");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::PACKED);
	const unsigned UnsignedPackedAlpha = g_Config.m_QmTranslateBtnColorDisabled;
	EXPECT_NE(UnsignedPackedAlpha & NTranslateUiSettings::COLOR_ALPHA_MASK, 0u);
	EXPECT_EQ(MigrateDisabledColor(), UnsignedPackedAlpha);

	pConsole->ExecuteLine("qm_translate_btn_color_disabled +2153407356");
	EXPECT_EQ(pConfigManager->ColorValueInputAlphaMode("qm_translate_btn_color_disabled"), EColorInputAlphaMode::PACKED);
	EXPECT_EQ(MigrateDisabledColor(), g_Config.m_QmTranslateBtnColorDisabled);
}

TEST(QmTranslateUiSettings, MigrationMarkerPreservesIntentionalTransparentColor)
{
	bool Migrated = true;
	unsigned Disabled = 0x005A6B7Cu;
	unsigned Enabled = 0x00010203u;
	unsigned Background = 0x00A1B2C3u;
	unsigned Selected = 0x00000000u;
	unsigned Normal = 0x00D4E5F6u;
	EXPECT_FALSE(NTranslateUiSettings::MigrateLegacyColorAlphas(Migrated, Disabled, Enabled, Background, Selected, Normal,
		DefaultConfig::QmTranslateBtnColorDisabled, DefaultConfig::QmTranslateBtnColorEnabled, DefaultConfig::QmTranslateMenuBgColor,
		DefaultConfig::QmTranslateMenuOptionSelected, DefaultConfig::QmTranslateMenuOptionNormal));
	EXPECT_TRUE(Migrated);
	EXPECT_EQ(Disabled, 0x005A6B7Cu);
	EXPECT_EQ(Enabled, 0x00010203u);
	EXPECT_EQ(Background, 0x00A1B2C3u);
	EXPECT_EQ(Selected, 0x00000000u);
	EXPECT_EQ(Normal, 0x00D4E5F6u);
}
